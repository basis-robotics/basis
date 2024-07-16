#include <filesystem>
#include <string_view>
#include <unistd.h>

#include <linux/prctl.h>  /* Definition of PR_* constants */
#include <sys/prctl.h>

#include <basis/unit.h>
#include <basis/recorder.h>
#include <basis/recorder/protobuf_log.h>

#include "launch.h"
#include "launch_definition.h"
#include "process_manager.h"
#include "unit_loader.h"

namespace {
/**
 * Search for a unit in well known directories given a unit name
 * @param unit_name
 * @return std::optional<std::filesystem::path>
 */
std::optional<std::filesystem::path> FindUnit(std::string_view unit_name) {
    const std::filesystem::path basis_unit_dir = "/opt/basis/unit/";

    std::filesystem::path so_path = basis_unit_dir / (std::string(unit_name) + ".unit.so");

    spdlog::debug("Searching for unit {} at {}", unit_name, so_path.string());
    if(std::filesystem::is_regular_file(so_path)) {
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
        for(auto& thread : threads) {
            thread.join();
        }
    }

    /**
     * Run a process given a definition - will iterate over each unit in the definition, load, and run.
     * @param process
     * @param recorder
     * @return bool
     */
    bool RunProcess(const ProcessDefinition& process, basis::RecorderInterface* recorder) {
        spdlog::info("Running process with {} units", process.units.size());

        for(const auto& [unit_name, unit] : process.units) {
            std::optional<std::filesystem::path> unit_so_path = FindUnit(unit.unit_type);
            if(unit_so_path) {
                if(!LaunchSharedObjectInThread(*unit_so_path, unit_name, recorder)) {
                    return false;
                }
            }
            else {
                spdlog::error("Failed to find unit type {}", unit.unit_type);
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

    bool LaunchSharedObjectInThread(const std::filesystem::path& path, std::string_view unit_name, basis::RecorderInterface* recorder) {
        std::unique_ptr<basis::Unit> unit(CreateUnit(path, unit_name));

        if(!unit) {
            return false;
        }

        threads.emplace_back([this, unit = std::move(unit), path = path.string(), recorder]() mutable {
            spdlog::info("Started thread with unit {}", path);
            UnitThread(unit.get(), recorder);
        });
        return true;
    }

protected:
    /**
     * The thread for running the unit. Will probably be replaced with a shared helper later, this block of code is duplicated three times now.
     * @param unit
     */
    void UnitThread(basis::Unit* unit, basis::RecorderInterface* recorder) {
        unit->WaitForCoordinatorConnection();
        unit->CreateTransportManager(recorder);
        unit->Initialize();

        while (!stop) {
            unit->Update(basis::core::Duration::FromSecondsNanoseconds(1, 0));
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
[[nodiscard]] Process CreateSublauncherProcess(const std::string& process_name, const std::vector<std::string>& args) {
  assert(args.size() >= 3);

  // Construct new arguments to pass in
  std::vector<const char*> args_copy;

  // basis
  args_copy.push_back(args[0].data());
  // launch
  args_copy.push_back(args[1].data());
  args_copy.push_back("--process");
  args_copy.push_back(process_name.data());
  // <the args>
  for(size_t i = 2; i < args.size(); i++) {
    args_copy.push_back(args[i].data());
  }
  // null terminator for argv
  args_copy.push_back(nullptr);

  int pid = fork();
  if(pid == -1) {
    spdlog::error("Error {} launching {}", strerror(errno), process_name);
  }
  else if(pid == 0) {
    // It's unsafe to do any allocations here - we may have forked while malloc() was locked

    // die when the parent dies
    // todo: might want to assert we are main thread here. SIGHUP will kill the thread
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    // If our parent already died, die anyhow
    if (getppid() == 1) {
        exit(1);
    }
    execv(args[0].data(), const_cast<char**>(args_copy.data()));
    // Manually print to stderr, don't trust anything
    int error = errno;
    fputs("Failed to execv " , stderr);
    fputs(args[0].data(), stderr);
    fputs(" ", stderr);
    fputs(strerror(error), stderr);
    exit(1);
  }
  else {
    spdlog::debug("forked with pid {}", pid);
  }

  return Process(pid);
}

std::atomic<bool> global_stop = false;
void SignalHandler([[maybe_unused]] int signal) {
    global_stop = true;
}

void InstallSignalHandler(int sig) {
    struct sigaction act;
    memset(&act,0,sizeof(act));
    act.sa_handler = SignalHandler;
    // Note: for now, we can't use SA_RESETHAND here - SIGINT may be delivered multiple times
    // todo: use SIGUSR1 as custom signal to children
    // todo: on SIGHUP, install background timer to suicide anyhow
    // todo: on parent process, track number of SIGINT and hard kill if repeated
    act.sa_flags = 0;

    sigaction(sig, &act, NULL);
}

/**
 * Launch a yaml
 * @param launch
 * @param args
 */
void LaunchYaml(const LaunchDefinition& launch, const std::vector<std::string>& args) {
    std::vector<Process> managed_processes;
    for(const auto& [process_name, _] : launch.processes) {
      managed_processes.push_back(CreateSublauncherProcess(process_name, args));
    }

    InstallSignalHandler(SIGINT);

    // Sleep until signal
    // TODO: this can be a condition variable now
    while(!global_stop) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    spdlog::info("Top level launcher got kill signal, killing children.");

    // Send signal to all processes
    for(Process& process : managed_processes) {
        process.Kill(SIGINT);
    }

    // TODO: we could just managed_processes.clear() with the same effect
    for(Process& process : managed_processes) {
      bool killed = process.Wait(5);
      if(!killed) {
        spdlog::error("Failed to kill pid {}", process.GetPid());
      }
    }
}

}


// todo: probably take a std::fs::path here
void LaunchYamlPath(std::string_view yaml_path, const std::vector<std::string>& args, std::string process_name_filter) {
    YAML::Node loaded_yaml = YAML::LoadFile(std::string(yaml_path));

    const LaunchDefinition launch = ParseLaunchDefinitionYAML(loaded_yaml);

    if(process_name_filter.empty()) {
        // We are the parent launcher, will fork here
        LaunchYaml(launch, args);
    }
    else {
        // We are a child launcher
        std::unique_ptr<basis::RecorderInterface> recorder;
        if(launch.recording_settings && launch.recording_settings->patterns.size()) {
            std::string recorder_type;
            if(launch.recording_settings->async) {
                recorder_type = " (async)";
                recorder = std::make_unique<basis::AsyncRecorder>(launch.recording_settings->directory, launch.recording_settings->patterns);
            }
            else {
                recorder = std::make_unique<basis::Recorder>(launch.recording_settings->directory, launch.recording_settings->patterns);
            }
            std::string record_name = fmt::format("{}_{}", process_name_filter, basis::core::MonotonicTime::Now().ToSeconds());

            spdlog::info("Recording{} to {}.mcap", recorder_type, (launch.recording_settings->directory / record_name).string());

            recorder->Start(record_name);
        }

        std::unique_ptr<basis::core::transport::CoordinatorConnector> system_coordinator_connector;
        while (!system_coordinator_connector) {
            system_coordinator_connector = basis::core::transport::CoordinatorConnector::Create();
            if (!system_coordinator_connector) {
                spdlog::warn("No connection to the coordinator, waiting 1 second and trying again");
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

        InstallSignalHandler(SIGINT);
        InstallSignalHandler(SIGHUP);

        // Used for things like /log and /time
        auto system_transport_manager = basis::CreateStandardTransportManager(recorder.get());
        basis::CreateLogHandler(*system_transport_manager);

        {
            UnitExecutor runner;
            runner.RunProcess(launch.processes.at(process_name_filter), recorder.get());

            // Sleep until signal
            // TODO: this can be a condition variable now
            while(!global_stop) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                basis::StandardUpdate(system_transport_manager.get(), system_coordinator_connector.get());
            }

            spdlog::info("{} got kill signal, exiting...", process_name_filter);
        }

        basis::DestroyLogHandler();
    }
}