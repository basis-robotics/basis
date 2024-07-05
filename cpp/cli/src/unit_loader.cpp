#include "unit_loader.h"
#include <dlfcn.h>

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
    void *handle = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
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
