#include <string_view>
#include <yaml-cpp/yaml.h>
#include <dlfcn.h>
#include <basis/unit.h>
#include <unistd.h>

#include "process_manager.h"

struct DlClose {
  void operator()(void* handle) {
    dlclose(handle);
  }
};

using ManagedSharedObject = std::unique_ptr<void, DlClose>;

using CreateUnitCallback = basis::Unit*(*)();

struct UnitLoader {
  ManagedSharedObject handle;
  CreateUnitCallback create_unit;
};

std::unordered_map<std::string, UnitLoader> unit_loaders;

std::unique_ptr<basis::Unit> CreateUnit([[maybe_unused]] const std::filesystem::path& path) {
  std::string string_path = path.string();

  auto maybe_unit_loader = unit_loaders.find(string_path);
  if(maybe_unit_loader == unit_loaders.end()) {
    void *handle = dlopen(path.c_str(), RTLD_NOW | RTLD_DEEPBIND);
    if (!handle) {
      std::cerr << "Failed to dlopen " << path << std::endl;
      std::cerr << dlerror() << std::endl;
      return nullptr;
    }

    ManagedSharedObject managed_handle(handle);

    auto load_unit = (CreateUnitCallback)dlsym(handle, "CreateUnit");
    if (!load_unit) {
      std::cerr << "Failed to find unit interface CreateUnit in " << path << std::endl;
      return nullptr;
    }
    maybe_unit_loader = unit_loaders.emplace(path.string(), UnitLoader{std::move(managed_handle), load_unit}).first;
  }

  return std::unique_ptr<basis::Unit>(maybe_unit_loader->second.create_unit());
}

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


[[nodiscard]] Process LaunchSublauncher(const std::string& process_name, const std::vector<std::string>& args) {
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
      managed_processes.push_back(LaunchSublauncher(node.first.as<std::string>(), args));
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