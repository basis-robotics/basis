#include <filesystem>
#include <string_view>
#include <basis/unit.h>
#include <unistd.h>

#include <linux/prctl.h>  /* Definition of PR_* constants */
#include <sys/prctl.h>

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
     * @return bool 
     */
    bool RunProcess(const ProcessDefinition& process) {
        spdlog::info("Running process with {} units", process.units.size());

        for(const auto& [unit_name, unit] : process.units) {
            std::optional<std::filesystem::path> unit_so_path = FindUnit(unit.unit_type);
            if(unit_so_path) {
                if(!LaunchSharedObjectInThread(*unit_so_path, unit_name)) {
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
    bool LaunchSharedObjectInThread(const std::filesystem::path& path, std::string_view unit_name) {
        std::unique_ptr<basis::Unit> unit(CreateUnit(path, unit_name));
        
        if(!unit) {
            return false;
        }

        threads.emplace_back([this, unit = std::move(unit), path]() mutable {
            spdlog::info("Started thread with unit {}", path.string());
            UnitThread(unit.get());
        });
        return true;
    }

protected:
    /**
     * The thread for running the unit. Will probably be replaced with a shared helper later, this block of code is duplicated three times now.
     * @param unit 
     */
    void UnitThread(basis::Unit* unit) {
        unit->WaitForCoordinatorConnection();
        unit->CreateTransportManager();
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

/**
 * Launch a 
 * @param launch 
 * @param args 
 */
void LaunchYaml(const LaunchDefinition& launch, const std::vector<std::string>& args) {
    std::vector<Process> managed_processes;
    for(const auto& [process_name, _] : launch.processes) {
      managed_processes.push_back(CreateSublauncherProcess(process_name, args));
    }
    for(Process& process : managed_processes) {
      bool killed = process.Wait();
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
        LaunchYaml(launch, args);
    }
    else {
        UnitExecutor runner;
        runner.RunProcess(launch.processes.at(process_name_filter));
        // Sleep until signal
        // todo: proper shutdown
        while(true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}