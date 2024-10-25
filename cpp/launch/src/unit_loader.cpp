#include <basis/launch/unit_loader.h>
#include <dlfcn.h>

#include <basis/unit/create_unit.h>

struct DlClose {
  void operator()(void *handle) { dlclose(handle); }
};

using ManagedSharedObject = std::unique_ptr<void, DlClose>;

using CreateUnitCallback = decltype(CreateUnit) *;

struct UnitLoader {
  ManagedSharedObject handle;
  CreateUnitCallback create_unit;
};

std::unordered_map<std::string, UnitLoader> unit_loaders;

std::unique_ptr<basis::Unit> CreateUnitWithLoader(const std::filesystem::path &path, std::string_view unit_name,
                                                  const basis::arguments::CommandLineTypes &command_line) {
  std::string string_path = path.string();

  auto maybe_unit_loader = unit_loaders.find(string_path);
  if (maybe_unit_loader == unit_loaders.end()) {
    // For now - need to use RTLD_GLOBAL to allow different inproc transports to communicate
    // This is the opposite of how the protobuf needs things - but no crashes on shutdown (yet?)
    void *handle = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
      std::cerr << "Failed to dlopen " << path << std::endl;
      std::cerr << "dlerror: " << dlerror() << std::endl;
      return nullptr;
    }

    ManagedSharedObject managed_handle(handle);

    dlerror();
    auto load_unit = reinterpret_cast<CreateUnitCallback>(dlsym(handle, "CreateUnit"));
    if (!load_unit) {
      std::cerr << "Failed to find unit interface CreateUnit in " << path << std::endl;
      std::cerr << "dlerror: " << dlerror() << std::endl;
      return nullptr;
    }
    maybe_unit_loader = unit_loaders.emplace(path.string(), UnitLoader{std::move(managed_handle), load_unit}).first;
  }

  return std::unique_ptr<basis::Unit>(maybe_unit_loader->second.create_unit(
      std::string(unit_name), command_line, [](const char *msg) { std::cerr << msg << std::endl; }));
}
