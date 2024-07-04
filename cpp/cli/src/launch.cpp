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

std::optional<std::filesystem::path> FindUnit(std::string_view unit_name) {
    const std::filesystem::path basis_unit_dir = "/opt/basis/unit/";

    std::filesystem::path so_path = basis_unit_dir / (std::string(unit_name) + ".unit.so");

    spdlog::debug("Searching for unit {} at {}", unit_name, so_path.string());
    if(std::filesystem::is_regular_file(so_path)) {
        return so_path;
    }
    return {};
}

class UnitExecutor {
public:
    UnitExecutor() {}
    ~UnitExecutor() {
        stop = true;
        for(auto& thread : threads) {
            thread.join();
        }
        spdlog::info("die");
    }
    bool RunProcess(const ProcessDefinition& process) {
        spdlog::info("Running process with {} units", process.units.size());

        for(const auto& [unit_name, unit] : process.units) {
            std::optional<std::filesystem::path> unit_so_path = FindUnit(unit.unit_type);
            if(unit_so_path) {
                if(!LaunchSharedObjectInThread(*unit_so_path)) {
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

    bool LaunchSharedObjectInThread([[maybe_unused]] const std::filesystem::path& path) {
        std::unique_ptr<basis::Unit> unit(CreateUnit(path));
        
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
    void UnitThread(basis::Unit* unit) {

        // todo: will definitely need to pass in object for inproc transport
        unit->WaitForCoordinatorConnection();
        unit->CreateTransportManager();
        unit->Initialize();

        while (!stop) {
            unit->Update(basis::core::Duration::FromSecondsNanoseconds(1, 0));
        }
    }

    std::atomic<bool> stop = false;
    std::vector<std::thread> threads;
};

[[nodiscard]] Process CreateSublauncherProcess(const std::string& process_name, const std::vector<std::string>& args) {
  assert(args.size() >= 3);

  std::vector<const char*> args_copy;

  args_copy.push_back(args[0].data());
  args_copy.push_back(args[1].data());

  args_copy.push_back("--process");
  args_copy.push_back(process_name.data());
  
  for(size_t i = 2; i < args.size(); i++) {
    args_copy.push_back(args[i].data());
  }
  args_copy.push_back(nullptr);

  // todo safe SIGHUP
  int pid = fork();
  if(pid == -1) {
    spdlog::error("Error {} launching {}", strerror(errno), process_name);
  }
  else if(pid == 0) {
    // die when the parent dies
    // todo: might want to assert we are main thread here
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    execv(args[0].data(), const_cast<char**>(args_copy.data()));
  }
  else {
    spdlog::debug("forked with pid {}", pid);
  }

  return Process(pid);
}

void LaunchYaml(const LaunchDefinition& launch, const std::vector<std::string>& args) {
    // todo: parse, then load?
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