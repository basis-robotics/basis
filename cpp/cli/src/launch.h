#include <string_view>
#include <yaml-cpp/yaml.h>
#include <dlfcn.h>
#include <basis/unit.h>

void LaunchSharedObject([[maybe_unused]] const std::filesystem::path& path) {
  void *handle = dlopen(path.c_str(), RTLD_NOW | RTLD_DEEPBIND);
  if (!handle) {
    std::cerr << "Failed to dlopen " << path << std::endl;
    std::cerr << dlerror() << std::endl;
    return;
  }

  using UnitLoader = basis::Unit*(*)();
  auto loader = (UnitLoader)dlsym(handle, "LoadUnit");
  if (!loader) {
    std::cerr << "Failed to find unit interface LoadUnit in " << path << std::endl;
    return;
  }
  std::unique_ptr<basis::Unit> unit(loader());

  unit->WaitForCoordinatorConnection();
  unit->CreateTransportManager();
  unit->Initialize();

  while (true) {
    unit->Update();
  }

  
/*
  using PluginCreator = T *(*)();
  auto Loader = (PluginCreator)dlsym(handle, "LoadPlugin");
  if (!Loader) {
    std::cerr << "Failed to find plugin interface LoadPlugin in " << path << std::endl;
    return nullptr;
  }
  std::unique_ptr<T> plugin(Loader());
*/
}

void FindUnit([[maybe_unused]] std::string_view unit_name) {

}

void LaunchYaml([[maybe_unused]] const YAML::Node& yaml) {
    // todo
    LaunchSharedObject("/opt/basis/unit/libunit_wip.so");
}

// todo: probably take a std::fs::path here
void LaunchYamlPath(std::string_view yaml_path) {
    std::cout << yaml_path << std::endl;

    YAML::Node loaded_yaml = YAML::LoadFile(std::string(yaml_path));
    LaunchYaml(loaded_yaml);
}