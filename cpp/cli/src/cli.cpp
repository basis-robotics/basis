/**
 * @file cli.cpp
 *
 * The main command line utility for basis. Used for querying the robot. See `basis --help` for more information.
 *
 */
#include <argparse/argparse.hpp>
#include <spdlog/spdlog.h>

#include <basis/core/coordinator_connector.h>
#include <basis/core/logging.h>
#include <basis/core/transport/transport_manager.h>

#include <filesystem>

#include <dlfcn.h>
#include <unistd.h>

#include "launch.h"

using namespace basis;

template<typename T>
std::unique_ptr<T> LoadPlugin(const char *path) {
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

template<typename T>
void LoadPluginsAtPath(std::filesystem::path search_path, std::unordered_map<std::string, std::unique_ptr<T>>& out) {
  if(!std::filesystem::exists(search_path)) {
    return;
  }
  for (auto entry : std::filesystem::directory_iterator(search_path)) {
    std::filesystem::path plugin_path = entry.path();
    if(entry.is_directory()) {
      plugin_path /= plugin_path.filename();
      plugin_path.replace_extension(".so");
    }
    
    std::unique_ptr<T> plugin(LoadPlugin<T>(plugin_path.string().c_str()));
    if(!plugin) {
      std::cerr << "Failed to load plugin at " << plugin_path.string() << std::endl;
      continue;
    }

    std::string name(plugin->GetPluginName());
    if(!out.contains(name)) {
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
template<typename T>
void LoadPlugins() {
  char buf[1024] = {};
  readlink("/proc/self/exe", buf, 1024);

  std::filesystem::path search_path = buf;
  search_path = search_path.parent_path().parent_path() / "plugins" / T::PLUGIN_TYPE;

  std::unordered_map<std::string, std::unique_ptr<T>> out;

  LoadPluginsAtPath<T>(search_path, out);
  LoadPluginsAtPath<T>(std::filesystem::path("/opt/basis/plugins") / T::PLUGIN_TYPE, out);

  serialization_plugins = std::move(out);
}


std::optional<basis::core::transport::proto::MessageSchema>
FetchSchema(const std::string &schema_id, basis::core::transport::CoordinatorConnector *connector, int timeout_s) {
  connector->RequestSchemas({&schema_id, 1});

  auto end = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_s);
  while (std::chrono::steady_clock::now() < end) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    connector->Update();
    auto schema_ptr = connector->TryGetSchema(schema_id);
    if (schema_ptr) {
      return *schema_ptr;
    }

    if (connector->errors_from_coordinator.size()) {
      for (const std::string &error : connector->errors_from_coordinator) {
        std::cerr << "Errors when fetching schema:" << std::endl;
        std::cerr << error << std::endl;
      }
      return {};
    }
  }

  std::cerr << "Timed out while fetching schema [" << schema_id << "]" << std::endl;

  return {};
}

void PrintTopic(const std::string &topic, basis::core::transport::CoordinatorConnector *connector,
                std::optional<size_t> max_num_messages, bool json) {
  basis::core::threading::ThreadPool work_thread_pool(4);

  auto *info = connector->GetLastNetworkInfo();

  auto it = info->publishers_by_topic().find(topic);
  if (it == info->publishers_by_topic().end()) {
    std::cerr << "No publishers for topic " << topic << std::endl;
    return;
  }

  std::string schema_id = it->second.publishers()[0].schema_id();
  std::optional<basis::core::transport::proto::MessageSchema> maybe_schema = FetchSchema(schema_id, connector, 5);
  if (!maybe_schema) {
    return;
  }

  auto plugin_it = serialization_plugins.find(maybe_schema->serializer());
  if (plugin_it == serialization_plugins.end()) {
    std::cerr << "Unknown serializer " << maybe_schema->serializer() << " please recompile basis_cli with support"
              << std::endl;
    return;
  }
  core::serialization::SerializationPlugin *plugin = plugin_it->second.get();

  plugin->LoadSchema(maybe_schema->name(), maybe_schema->schema());
  basis::core::transport::TransportManager transport_manager(
      std::make_unique<basis::core::transport::InprocTransport>());
  transport_manager.RegisterTransport("net_tcp", std::make_unique<basis::plugins::transport::TcpTransport>());

  // This looks dangerous to take as a reference but is actually safe -
  // the subscriber destructor will wait until the callback exits before the atomic goes out of scope
  std::atomic<size_t> num_messages;
  auto time_test_sub = transport_manager.SubscribeRaw(
      topic,
      [&]([[maybe_unused]] auto msg) {
        num_messages++;

        auto to_string = json ? &core::serialization::SerializationPlugin::DumpMessageJSONString
                              : &core::serialization::SerializationPlugin::DumpMessageString;

        auto maybe_string = (plugin->*to_string)(msg->GetPayload(), maybe_schema->name());
        if (maybe_string) {
          std::cout << *maybe_string << std::endl;
        }
      },
      &work_thread_pool, nullptr, {});

  while (!max_num_messages || max_num_messages > num_messages) {
    // todo: move this out into "unit"
    connector->SendTransportManagerInfo(transport_manager.GetTransportManagerInfo());
    connector->Update();

    if (connector->GetLastNetworkInfo()) {
      transport_manager.HandleNetworkInfo(*connector->GetLastNetworkInfo());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

void PrintSchema(const std::string &schema_name, basis::core::transport::CoordinatorConnector *connector) {
  std::optional<basis::core::transport::proto::MessageSchema> maybe_schema = FetchSchema(schema_name, connector, 5);
  if (maybe_schema) {
    std::cout << maybe_schema->serializer() << ":" << maybe_schema->name() << std::endl;
    std::cout << maybe_schema->schema() << std::endl;
  }
}

int main(int argc, char *argv[]) {
  core::logging::InitializeLoggingSystem();

  LoadPlugins<basis::core::serialization::SerializationPlugin>();

  argparse::ArgumentParser parser("basis");

  parser.add_argument("--port")
      .help("The port that the basis coordinator is listening at.")
      .scan<'i', uint16_t>()
      .default_value(BASIS_PUBLISH_INFO_PORT);

  // basis topic
  argparse::ArgumentParser topic_command("topic");
  topic_command.add_description("Topic information");
  // basis topic ls
  argparse::ArgumentParser topic_ls_command("ls");
  topic_ls_command.add_description("list the available topics");
  topic_command.add_subparser(topic_ls_command);
  // basis topic info
  argparse::ArgumentParser topic_info_command("info");
  topic_info_command.add_description("get a topic's information");
  topic_info_command.add_argument("topic");
  topic_command.add_subparser(topic_info_command);
  // basis topic print
  argparse::ArgumentParser topic_print_command("print");
  topic_print_command.add_description("print a message on the topic");
  topic_print_command.add_argument("topic");
  topic_print_command.add_argument("-n").scan<'i', size_t>().help("number of messages to print (default: infinite)");
  topic_print_command.add_argument("--json", "-j").help("dump this message as JSON").flag();
  topic_command.add_subparser(topic_print_command);

  parser.add_subparser(topic_command);

  // basis schema
  argparse::ArgumentParser schema_command("schema");
  schema_command.add_description("Schema information");
  // basis schema ls
  // todo
  // basis schema print
  argparse::ArgumentParser schema_print_command("print");
  schema_print_command.add_description("print a schema");
  schema_print_command.add_argument("schema");
  schema_command.add_subparser(schema_print_command);
  parser.add_subparser(schema_command);

  // todo
  // basis plugins ls

  // todo
  // basis unit ls?

  // basis launch
  argparse::ArgumentParser launch_command("launch");
  launch_command.add_description("launch a yaml");
  launch_command.add_argument("--process").help("The process to launch inside the yaml").default_value("");
  launch_command.add_argument("launch_yaml");
  parser.add_subparser(launch_command);

  try {
    parser.parse_args(argc, argv);
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << parser;
    return 1;
  }

  const uint16_t port = parser.get<uint16_t>("--port");

  auto connection = basis::core::transport::CoordinatorConnector::Create(port);
  if (!connection) {
    spdlog::error("Unable to connect to the basis coordinator at port {}", port);
    return 1;
  }

  auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!connection->GetLastNetworkInfo() && std::chrono::steady_clock::now() < end) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    connection->Update();
  }
  auto *info = connection->GetLastNetworkInfo();
  if (!info) {
    spdlog::error("Timed out waiting for network info from coordinator");
    return 1;
  }
  if (parser.is_subcommand_used("topic")) {
    if (topic_command.is_subcommand_used("ls")) {
      std::cout << "topics:" << std::endl;
      for (auto &[topic, pubs] : info->publishers_by_topic()) {
        std::cout << "  " << topic << " [" << pubs.publishers()[0].schema_id() << "] (" << pubs.publishers().size()
                  << " publisher)" << std::endl;
      }
    } else if (topic_command.is_subcommand_used("info")) {
      auto topic = topic_info_command.get("topic");

      auto it = info->publishers_by_topic().find(topic);
      if (it == info->publishers_by_topic().end()) {
        std::cerr << "No publishers for topic " << topic << std::endl;
        return 1;
      }
      std::cout << "topic: " << topic << std::endl;
      // TODO: should have a separate listing for types
      std::cout << "type: " << it->second.publishers()[0].schema_id() << std::endl;
      std::cout << std::endl;
      for (const auto &pub : (it->second.publishers())) {
        std::cout << "id: " << std::hex << pub.publisher_id_high() << pub.publisher_id_low() << std::dec << std::endl;
        std::cout << "  endpoints: " << std::endl;
        for (const auto &[transport, endpoint] : pub.transport_info()) {
          std::cout << "    " << transport << ": " << endpoint << std::endl;
        }
        std::cout << std::endl;
      }
    } else if (topic_command.is_subcommand_used("print")) {
      auto topic = topic_print_command.get("topic");
      PrintTopic(topic, connection.get(), topic_print_command.present<size_t>("-n"),
                 topic_print_command["--json"] == true);
    }
  } else if (parser.is_subcommand_used("schema")) {
    auto topic = schema_print_command.get("schema");
    if (schema_command.is_subcommand_used("print")) {
      PrintSchema(topic, connection.get());
    }
  } else if (parser.is_subcommand_used("launch")) {
    auto launch_yaml = launch_command.get("launch_yaml");
    std::vector<std::string> args(argv, argv + argc);
    LaunchYamlPath(launch_yaml, args, launch_command.get("--process"));
    
  }

  serialization_plugins.clear();

  return 0;
}