/**
 * @file cli.cpp
 *
 * The main command line utility for basis. Used for querying the robot. See `basis --help` for more information.
 *
 */
#include <argparse/argparse.hpp>
#include <spdlog/spdlog.h>

#include <basis/core/coordinator_connector.h>
#include <basis/core/transport/transport_manager.h>

// todo: load via plugin
#include <basis/plugins/serialization/protobuf.h>

#ifdef BASIS_ENABLE_ROS
#include <basis/plugins/serialization/rosmsg.h>

#endif

// todo: one map :)
using StringDumpCallback = std::function<std::optional<std::string>(std::span<const std::byte>, std::string_view)>;
std::unordered_map<std::string, StringDumpCallback> string_dumpers = {
    {"protobuf", basis::plugins::serialization::ProtobufSerializer::DumpMessageString},
#ifdef BASIS_ENABLE_ROS
    {"rosmsg", basis::plugins::serialization::RosmsgSerializer::DumpMessageString},
#endif
};

std::unordered_map<std::string, StringDumpCallback> json_dumpers = {
    {"protobuf", basis::plugins::serialization::ProtobufSerializer::DumpMessageJSONString},
#ifdef BASIS_ENABLE_ROS
    {"rosmsg", basis::plugins::serialization::RosmsgSerializer::DumpMessageJSONString},
#endif
};

std::unordered_map<std::string, std::function<bool(std::string_view, std::string_view)>> schema_loaders = {
    {"protobuf", basis::plugins::serialization::ProtobufSerializer::LoadSchema},
#ifdef BASIS_ENABLE_ROS
    {"rosmsg", basis::plugins::serialization::RosmsgSerializer::LoadSchema},
#endif
};

std::optional<basis::core::transport::proto::MessageSchema>
FetchSchema(const std::string &schema_id, basis::core::transport::CoordinatorConnector *connector, int timeout_s) {
  // TODO
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

  StringDumpCallback to_string;
  auto *callback_map = json ? &json_dumpers : &string_dumpers;

  auto to_string_it = callback_map->find(maybe_schema->serializer());
  if (to_string_it == callback_map->end()) {
    // Note: theoretically we could do this string dumping on the publisher side, instead
    std::cerr << "Unknown serializer " << maybe_schema->serializer() << " please recompile basis_cli with support"
              << std::endl;
    return;
  }

  schema_loaders[maybe_schema->serializer()](maybe_schema->name(), maybe_schema->schema());

  auto thread_pool_manager = std::make_shared<basis::core::transport::ThreadPoolManager>();

  basis::core::transport::TransportManager transport_manager(
      std::make_unique<basis::core::transport::InprocTransport>());
  transport_manager.RegisterTransport("net_tcp",
                                      std::make_unique<basis::plugins::transport::TcpTransport>(thread_pool_manager));

  // This looks dangerous to take as a reference but is actually safe -
  // the subscriber destructor will wait until the callback exits before the atomic goes out of scope
  std::atomic<size_t> num_messages;
  auto time_test_sub = transport_manager.SubscribeRaw(topic,
                                                      [&]([[maybe_unused]] auto msg) {
                                                        num_messages++;
                                                        // spdlog::info("Got message number {} of size {}",
                                                        // (size_t)num_messages, msg->GetPayload().size());
                                                        auto maybe_string = to_string_it->second(msg->GetPayload(),
                                                                                                 maybe_schema->name());
                                                        if (maybe_string) {
                                                          std::cout << *maybe_string << std::endl;
                                                        }
                                                      },
                                                      nullptr, {});

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
  }

  return 0;
}
