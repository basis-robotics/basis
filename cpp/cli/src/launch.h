#include <string_view>
#include <yaml-cpp/yaml.h>
#include <dlfcn.h>
#include <basis/unit.h>

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
  unit->WaitForCoordinatorConnection();
  unit->CreateTransportManager();
  unit->Initialize();

  while (true) {
    unit->Update(basis::core::Duration::FromSecondsNanoseconds(1, 0));
  }
}

void FindUnit([[maybe_unused]] std::string_view unit_name) {

}

void LaunchProcess(const std::string_view process_name, [[maybe_unused]] const YAML::Node& process) {
  // fork??
  spdlog::info("{}", process_name);

  // todo safe SIGHUP
  if(fork() == -1) {
    // basically we have to exec here as we have threads elsewhere
  }
  else {

  }
}

void LaunchYaml(const YAML::Node& yaml) {
    // todo
    //LaunchSharedObject("/opt/basis/unit/libunit_wip.so");
    const auto processes = yaml["processes"];
    for(const auto& node : processes) {

      LaunchProcess(node.first.as<std::string>(), node.second);
    }
}

// todo: probably take a std::fs::path here
void LaunchYamlPath(std::string_view yaml_path) {
    std::cout << yaml_path << std::endl;

    YAML::Node loaded_yaml = YAML::LoadFile(std::string(yaml_path));
    LaunchYaml(loaded_yaml);
}