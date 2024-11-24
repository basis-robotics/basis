/**
 * @file cli.cpp
 *
 * The main command line utility for basis. Used for querying the robot. See `basis --help` for more information.
 *
 */
#include "cli_launch.h"
#include "cli_schema.h"
#include "cli_topic.h"

#include <basis/cli_logger.h>

#include <argparse/argparse.hpp>
#include <filesystem>

#include <dlfcn.h>
#include <unistd.h>

namespace basis::cli {

template <typename T> std::unique_ptr<T> LoadPlugin(const char *path) {
  // Use RTLD_DEEPBIND to avoid the plugin sharing protobuf globals with us and crashing
  void *handle = dlopen(path, RTLD_NOW | RTLD_DEEPBIND);
  // todo: this handle is leaked
  if (!handle) {
    std::cerr << "Failed to dlopen " << path << std::endl;
    return nullptr;
  }

  using PluginCreator = T *(*)();
  auto Loader = (PluginCreator)dlsym(handle, "LoadPlugin");
  if (!Loader) {
    std::cerr << "Failed to find plugin interface LoadPlugin in " << path << std::endl;
    return nullptr;
  }
  std::unique_ptr<T> plugin(Loader());

  return plugin;
}

template <typename T>
void LoadPluginsAtPath(std::filesystem::path search_path, std::unordered_map<std::string, std::unique_ptr<T>> &out) {
  if (!std::filesystem::exists(search_path)) {
    return;
  }
  for (auto entry : std::filesystem::directory_iterator(search_path)) {
    std::filesystem::path plugin_path = entry.path();
    if (entry.is_directory()) {
      plugin_path /= plugin_path.filename();
      plugin_path.replace_extension(".so");
    }

    std::unique_ptr<T> plugin(LoadPlugin<T>(plugin_path.string().c_str()));
    if (!plugin) {
      std::cerr << "Failed to load plugin at " << plugin_path.string() << std::endl;
      continue;
    }

    std::string name(plugin->GetPluginName());
    if (!out.contains(name)) {
      out.emplace(name, std::move(plugin));
    }
  }
}

std::unordered_map<std::string, std::unique_ptr<core::serialization::SerializationPlugin>> serialization_plugins;

/**
 * Load all plugins of a type
 *
 * @todo: this won't work well for things other than cli - it's assuming the build system layout, for one
 */
template <typename T> void LoadPlugins() {
  char buf[1024] = {};
  readlink("/proc/self/exe", buf, 1024);

  std::filesystem::path search_path = buf;
  search_path = search_path.parent_path().parent_path() / "plugins" / T::PLUGIN_TYPE;

  std::unordered_map<std::string, std::unique_ptr<T>> out;

  LoadPluginsAtPath<T>(search_path, out);
  LoadPluginsAtPath<T>(std::filesystem::path("/opt/basis/plugins") / T::PLUGIN_TYPE, out);

  serialization_plugins = std::move(out);
}

std::unique_ptr<basis::core::transport::CoordinatorConnector> CLISubcommand::CreateCoordinatorConnector(uint16_t port) {
  auto connector = basis::core::transport::CoordinatorConnector::Create(port);
  if (!connector) {
    BASIS_LOG_ERROR("Unable to connect to the basis coordinator at port {}", port);
  }
  return connector;
}

} // namespace basis::cli

int main(int argc, char *argv[]) {

  using namespace basis;
  using namespace basis::cli;

  core::logging::InitializeLoggingSystem();

  LoadPlugins<basis::core::serialization::SerializationPlugin>();

  argparse::ArgumentParser parser("basis");

  parser.add_argument("--port")
      .help("The port that the basis coordinator is listening at.")
      .scan<'i', uint16_t>()
      .default_value(BASIS_PUBLISH_INFO_PORT);

  // basis topic
  TopicCommand topic_command(parser);

  // basis schema
  SchemaCommand schema_command(parser);

  // todo
  // basis plugins ls

  // todo
  // basis unit ls?

  // basis launch
  LaunchCommand launch_command(parser);

  try {
    parser.parse_args(argc, argv);
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << parser;
    return 1;
  }

  const uint16_t port = parser.get<uint16_t>("--port");

  bool ok = false;
  if (topic_command.IsInUse()) {
    ok = topic_command.HandleArgs(port);
  } else if (schema_command.IsInUse()) {
    ok = schema_command.HandleArgs(port);
  } else if (launch_command.IsInUse()) {
    ok = launch_command.HandleArgs(argc, argv);
  }

  serialization_plugins.clear();

  return ok ? 0 : 1;
}