#include <filesystem>
#include <memory>
#include <string_view>
#include <unistd.h>

#include <linux/prctl.h> /* Definition of PR_* constants */
#include <sys/prctl.h>

#include <basis/recorder.h>
#include <basis/recorder/protobuf_log.h>
#include <basis/unit.h>

#include <google/protobuf/wrappers.pb.h>

#include "basis/cli_logger.h"
#include "basis/core/logging/macros.h"
#include "basis/core/time.h"
#include "process_manager.h"
#include <basis/launch.h>
#include <basis/launch/launch_definition.h>
#include <basis/launch/unit_loader.h>

#include <time.pb.h>

#include "backward.hpp"

DECLARE_AUTO_LOGGER_NS(basis::launch)

namespace backward {

backward::SignalHandling sh;

} // namespace backward

namespace basis::launch {

/**
 * Search for a unit in well known directories given a unit name
 * @param unit_name
 * @return std::optional<std::filesystem::path>
 */
std::optional<std::filesystem::path> FindUnit(std::string_view unit_name) {
  const std::filesystem::path basis_unit_dir = "/opt/basis/unit/";

  std::filesystem::path so_path = basis_unit_dir / (std::string(unit_name) + ".unit.so");

  BASIS_LOG_DEBUG("Searching for unit {} at {}", unit_name, so_path.string());
  if (std::filesystem::is_regular_file(so_path)) {
    return so_path;
  }
  return {};
}
/**
 * Responsible for loading and running units
 */
class UnitExecutor {
public:
  UnitExecutor() {}
  ~UnitExecutor() {
    stop = true;
    for (auto &thread : threads) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }

  /**
   * Run a process given a definition - will iterate over each unit in the definition, load, and run.
   * @param process
   * @param recorder
   * @return bool
   */
  bool RunProcess(const ProcessDefinition &process, basis::RecorderInterface *recorder) {
    BASIS_LOG_INFO("Running process with {} units", process.units.size());

    for (const auto &[unit_name, unit] : process.units) {
      std::optional<std::filesystem::path> unit_so_path = FindUnit(unit.unit_type);
      if (unit_so_path) {
        if (!LaunchSharedObjectInThread(*unit_so_path, unit_name, recorder, unit.args)) {
          return false;
        }
      } else {
        BASIS_LOG_ERROR("Failed to find unit type {}", unit.unit_type);
        return false;
      }
    }
    return true;
  }

  /**
   * Given a path to a unit shared object, create the unit and run its main loop in a separate thread
   * @param path
   * @return bool If the launch was successful or not
   */

  bool LaunchSharedObjectInThread(const std::filesystem::path &path, std::string_view unit_name,
                                  basis::RecorderInterface *recorder,
                                  const basis::arguments::CommandLineTypes &command_line) {
    std::unique_ptr<basis::Unit> unit(CreateUnitWithLoader(path, unit_name, command_line));

    if (!unit) {
      return false;
    }

    threads.emplace_back([this, unit = std::move(unit), path = path.string(), recorder]() mutable {
      BASIS_LOG_INFO("Started thread with unit {}", path);
      UnitThread(unit.get(), recorder);
    });
    return true;
  }

protected:
  /**
   * The thread for running the unit. Will probably be replaced with a shared helper later, this block of code is
   * duplicated three times now.
   * @param unit
   */
  void UnitThread(basis::Unit *unit, basis::RecorderInterface *recorder) {
    unit->WaitForCoordinatorConnection();
    unit->CreateTransportManager(recorder);
    unit->Initialize();

    while (!stop) {
      unit->Update(&stop, basis::core::Duration::FromSecondsNanoseconds(1, 0));
    }
  }

  /**
   * Flag to stop all units from running.
   */
  std::atomic<bool> stop = false;
  /**
   * The threads running the main loop for each unit.
   */
  std::vector<std::thread> threads;
};

/**
 * Fork to create a new launcher
 * @param process_name The process in the yaml to run post-fork.
 * @param args The args passed into this launch.
 * @return Process A handle to the process we ran. When destructed, will kill the process.
 */
[[nodiscard]] cli::Process CreateSublauncherProcess(const std::string &process_name,
                                                    const std::vector<std::string> &args) {
  assert(args.size() >= 3);

  // Construct new arguments to pass in
  std::vector<const char *> args_copy;

  // basis
  char execv_target[1024] = {};
  readlink("/proc/self/exe", execv_target, sizeof(execv_target));
  args_copy.push_back(execv_target);

  // launch
  args_copy.push_back(args[1].data());
  args_copy.push_back("--process");
  args_copy.push_back(process_name.data());
  // <the args>
  for (size_t i = 2; i < args.size(); i++) {
    args_copy.push_back(args[i].data());
  }
  // null terminator for argv
  args_copy.push_back(nullptr);

  int pid = fork();
  if (pid == -1) {
    BASIS_LOG_ERROR("Error {} launching {}", strerror(errno), process_name);
  } else if (pid == 0) {
    // It's unsafe to do any allocations here - we may have forked while malloc() was locked

    // die when the parent dies
    // todo: might want to assert we are main thread here. SIGHUP will kill the thread
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    // If our parent already died, die anyhow
    if (getppid() == 1) {
      exit(1);
    }
    execv(execv_target, const_cast<char **>(args_copy.data()));
    // Manually print to stderr, don't trust anything
    int error = errno;
    fputs("Failed to execv ", stderr);
    fputs(execv_target, stderr);
    fputs(" ", stderr);
    fputs(strerror(error), stderr);
    exit(1);
  } else {
    BASIS_LOG_DEBUG("forked with pid {}", pid);
  }

  return cli::Process(pid);
}

std::atomic<bool> global_stop = false;
void SignalHandler([[maybe_unused]] int signal) { global_stop = true; }

void InstallSignalHandler(int sig) {
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = SignalHandler;
  // Note: for now, we can't use SA_RESETHAND here - SIGINT may be delivered multiple times
  // todo: use SIGUSR1 as custom signal to children
  // todo: on SIGHUP, install background timer to suicide anyhow
  // todo: on parent process, track number of SIGINT and hard kill if repeated
  act.sa_flags = 0;

  sigaction(sig, &act, NULL);
}

/**
 * Launch a yaml, forking for child processes
 */
void LaunchWithProcessForks(const LaunchDefinition &launch, const std::vector<std::string> &args) {
  std::vector<cli::Process> managed_processes;
  for (const auto &[process_name, _] : launch.processes) {
    managed_processes.push_back(CreateSublauncherProcess(process_name, args));
  }

  InstallSignalHandler(SIGINT);

  // Sleep until signal
  // TODO: this can be a condition variable now
  while (!global_stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  BASIS_LOG_INFO("Top level launcher got kill signal, killing children.");

  // Send signal to all processes
  for (cli::Process &process : managed_processes) {
    process.Kill(SIGINT);
  }

  // TODO: we could just managed_processes.clear() with the same effect
  for (cli::Process &process : managed_processes) {
    bool killed = process.Wait(5);
    if (!killed) {
      BASIS_LOG_ERROR("Failed to kill pid {}", process.GetPid());
    }
  }
}

/**
 * Launch a single process within a launch definition
 */
void LaunchProcessDefinition(const ProcessDefinition &process_definition,
                             const std::optional<RecordingSettings> &recording_settings,
                             const std::string_view process_name_filter, const bool sim) {
  while (!global_stop) {
    // We are a child launcher
    std::unique_ptr<basis::RecorderInterface> recorder;
    if (recording_settings && recording_settings->patterns.size()) {
      std::string recorder_type;
      if (recording_settings->async) {
        recorder_type = " (async)";
        recorder = std::make_unique<basis::AsyncRecorder>(recording_settings->directory, recording_settings->patterns);
      } else {
        recorder = std::make_unique<basis::Recorder>(recording_settings->directory, recording_settings->patterns);
      }

      std::string record_name =
          fmt::format("{}_{}", process_name_filter, basis::core::MonotonicTime::Now(true).ToSeconds());
      if (sim) {
        // TODO: it would be great to get the actual simulation start time, but then we won't be able to start logging
        // until coordinator connection this might be okay in practice
        record_name += "_sim";
      }

      BASIS_LOG_INFO("Recording{} to {}.mcap", recorder_type, (recording_settings->directory / record_name).string());

      recorder->Start(record_name);
    }

    std::unique_ptr<basis::core::transport::CoordinatorConnector> system_coordinator_connector;
    while (!system_coordinator_connector) {
      system_coordinator_connector = basis::core::transport::CoordinatorConnector::Create();
      if (!system_coordinator_connector) {
        BASIS_LOG_WARN("No connection to the coordinator, waiting 1 second and trying again");
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }

    InstallSignalHandler(SIGINT);
    InstallSignalHandler(SIGHUP);

    // Used for things like /log and /time
    auto system_transport_manager = basis::CreateStandardTransportManager(recorder.get());
    basis::CreateLogHandler(*system_transport_manager);

    basis::core::threading::ThreadPool time_thread_pool(1);
    auto time_subscriber = system_transport_manager->SubscribeCallable<basis::core::transport::proto::Time>(
        "/time",
        [](std::shared_ptr<const basis::core::transport::proto::Time> msg) {
          basis::core::MonotonicTime::SetSimulatedTime(msg->nsecs(), msg->run_token());
        },
        &time_thread_pool);

    {
      uint64_t token = basis::core::MonotonicTime::GetRunToken();
      while (sim && !token) {
        BASIS_LOG_INFO("In simulation mode, waiting on /time");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        basis::StandardUpdate(system_transport_manager.get(), system_coordinator_connector.get());
        token = basis::core::MonotonicTime::GetRunToken();
      }

      UnitExecutor runner;
      if (!runner.RunProcess(process_definition, recorder.get())) {
        BASIS_LOG_FATAL("Failed to launch process {}, will exit.", process_name_filter);
        global_stop = true;
      }

      // Sleep until signal
      // TODO: this can be a condition variable now
      while (!global_stop && token == basis::core::MonotonicTime::GetRunToken()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        basis::StandardUpdate(system_transport_manager.get(), system_coordinator_connector.get());
      }
      if (global_stop) {
        BASIS_LOG_INFO("{} got kill signal, exiting...", process_name_filter);
      } else {
        BASIS_LOG_INFO("{} detected playback restart, restarting...", process_name_filter);
      }
    }

    basis::DestroyLogHandler();
  }
}

void LaunchYamlDefinition(const LaunchDefinition &launch, const LaunchContext &context) {
  if (context.process_filter.empty()) {
    // We are the parent launcher, will fork here
    LaunchWithProcessForks(launch, context.all_args);
  } else {
    ProcessDefinition definition = launch.processes.at(context.process_filter);

    LaunchProcessDefinition(definition, launch.recording_settings, context.process_filter, context.sim);
  }
}

} // namespace basis::launch