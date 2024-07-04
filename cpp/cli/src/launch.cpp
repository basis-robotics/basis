#include <filesystem>
#include <string_view>
#include <yaml-cpp/yaml.h>
#include <basis/unit.h>
#include <unistd.h>

#include <linux/prctl.h>  /* Definition of PR_* constants */
#include <sys/prctl.h>

#include "launch.h"
#include "process_manager.h"
#include "unit_loader.h"

void LaunchSharedObject([[maybe_unused]] const std::filesystem::path& path) {
  std::unique_ptr<basis::Unit> unit(CreateUnit(path));
  if(!unit) {
    return;
  }
  // TODO: consider passing the coordinator connection in
  // ...it may have threading bugs, better not, for now
  unit->WaitForCoordinatorConnection();
  unit->CreateTransportManager();
  unit->Initialize();

  while (true) {
    unit->Update(basis::core::Duration::FromSecondsNanoseconds(1, 0));
  }
}

void FindUnit([[maybe_unused]] std::string_view unit_name) {

}

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

void LaunchYaml(const YAML::Node& yaml, const std::vector<std::string>& args) {
    // todo
    //LaunchSharedObject("/opt/basis/unit/libunit_wip.so");
    std::vector<Process> managed_processes;
    const auto processes = yaml["processes"];
    for(const auto& node : processes) {
      managed_processes.push_back(CreateSublauncherProcess(node.first.as<std::string>(), args));
    }
    for(Process& process : managed_processes) {
      bool killed = process.Wait();
      if(!killed) {
        spdlog::error("Failed to kill pid {}", process.GetPid());
      }
    }
}

// todo: probably take a std::fs::path here
void LaunchYamlPath(std::string_view yaml_path, const std::vector<std::string>& args) {
    std::cout << yaml_path << std::endl;

    YAML::Node loaded_yaml = YAML::LoadFile(std::string(yaml_path));
    LaunchYaml(loaded_yaml, args);
}