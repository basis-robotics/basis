#pragma once
#include "argparse/argparse.hpp"
#include "cli_subcommand.h"
#include "fetch_schema.h"
#include <basis/core/coordinator_connector.h>
#include <transport.pb.h>

namespace basis::cli {
extern std::unordered_map<std::string, std::unique_ptr<core::serialization::SerializationPlugin>> serialization_plugins;
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
  std::atomic<size_t> num_messages = 0;
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

bool RateTopic(const std::string &topic, basis::core::transport::CoordinatorConnector *connector) {
  basis::core::threading::ThreadPool work_thread_pool(4);

  auto *info = connector->GetLastNetworkInfo();

  auto it = info->publishers_by_topic().find(topic);
  if (it == info->publishers_by_topic().end()) {
    std::cerr << "No publishers for topic " << topic << std::endl;
    return false;
  }

  std::string schema_id = it->second.publishers()[0].schema_id();
  std::optional<basis::core::transport::proto::MessageSchema> maybe_schema = FetchSchema(schema_id, connector, 5);
  if (!maybe_schema) {
    return false;
  }

  auto plugin_it = serialization_plugins.find(maybe_schema->serializer());
  if (plugin_it == serialization_plugins.end()) {
    std::cerr << "Unknown serializer " << maybe_schema->serializer() << " please recompile basis_cli with support"
              << std::endl;
    return false;
  }
  core::serialization::SerializationPlugin *plugin = plugin_it->second.get();

  plugin->LoadSchema(maybe_schema->name(), maybe_schema->schema());
  basis::core::transport::TransportManager transport_manager(
      std::make_unique<basis::core::transport::InprocTransport>());
  transport_manager.RegisterTransport("net_tcp", std::make_unique<basis::plugins::transport::TcpTransport>());

  // This looks dangerous to take as a reference but is actually safe -
  // the subscriber destructor will wait until the callback exits before the atomic goes out of scope
  std::atomic<size_t> num_messages = 0;
  auto time_test_sub = transport_manager.SubscribeRaw(topic, [&]([[maybe_unused]] auto msg) { num_messages++; },
                                                      &work_thread_pool, nullptr, {});

  while (true) {
    // todo: move this out into "unit"
    connector->SendTransportManagerInfo(transport_manager.GetTransportManagerInfo());
    connector->Update();

    if (connector->GetLastNetworkInfo()) {
      transport_manager.HandleNetworkInfo(*connector->GetLastNetworkInfo());
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << num_messages << " Hz" << std::endl;
    num_messages = 0;
  }
  return true;
}

class TopicLsCommand : public CLISubcommand {
public:
  TopicLsCommand(argparse::ArgumentParser &parent_parser) : CLISubcommand("ls", parent_parser) {
    parser.add_description("list the available topics");
    Commit();
  }

  bool HandleArgs(const basis::core::transport::proto::NetworkInfo *network_info) {
    std::cout << "topics:" << std::endl;
    for (auto &[topic, pubs] : network_info->publishers_by_topic()) {
      std::cout << "  " << topic << " [" << pubs.publishers()[0].schema_id() << "] (" << pubs.publishers().size()
                << " publisher)" << std::endl;
    }
    return true;
  }
};

class TopicInfoCommand : public CLISubcommand {
public:
  TopicInfoCommand(argparse::ArgumentParser &parent_parser) : CLISubcommand("info", parent_parser) {
    parser.add_description("get a topic's information");
    parser.add_argument("topic");
    Commit();
  }

  bool HandleArgs(const basis::core::transport::proto::NetworkInfo *network_info) {
    std::string topic = parser.get("topic");

    auto it = network_info->publishers_by_topic().find(topic);
    if (it == network_info->publishers_by_topic().end()) {
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
    return true;
  }
};

class TopicPrintCommand : public CLISubcommand {
public:
  TopicPrintCommand(argparse::ArgumentParser &parent_parser) : CLISubcommand("print", parent_parser) {
    parser.add_description("print a message on the topic");
    parser.add_argument("topic");
    parser.add_argument("-n").scan<'i', size_t>().help("number of messages to print (default: infinite)");
    parser.add_argument("--json", "-j").help("dump this message as JSON").flag();
    Commit();
  }

  bool HandleArgs(basis::core::transport::CoordinatorConnector *connector) {
    std::string topic = parser.get("topic");
    PrintTopic(topic, connector, parser.present<size_t>("-n"), parser["--json"] == true);
    return true;
  }
};

class TopicHzCommand : public CLISubcommand {
public:
  TopicHzCommand(argparse::ArgumentParser &parent_parser) : CLISubcommand("hz", parent_parser) {
    // basis topic hz
    parser.add_description("check the receive rate of a topic");
    parser.add_argument("topic");

    Commit();
  }

  bool HandleArgs(basis::core::transport::CoordinatorConnector *connector) {
    return RateTopic(parser.get("topic"), connector);
  }
};

class TopicCommand : public CLISubcommand {
public:
  TopicCommand(argparse::ArgumentParser &parent_parser)
      : CLISubcommand("topic", parent_parser), topic_ls_command(parser), topic_info_command(parser),
        topic_print_command(parser), topic_hz_command(parser) {
    parser.add_description("Topic information");

    Commit();
  }

  bool HandleArgs(const basis::core::transport::proto::NetworkInfo *network_info,
                  basis::core::transport::CoordinatorConnector *connector) {
    if (topic_ls_command.IsInUse()) {
      return topic_ls_command.HandleArgs(network_info);
    } else if (topic_info_command.IsInUse()) {
      return topic_info_command.HandleArgs(network_info);
    } else if (topic_print_command.IsInUse()) {
      return topic_print_command.HandleArgs(connector);
    } else if (topic_hz_command.IsInUse()) {
      return topic_hz_command.HandleArgs(connector);
    }
    return false;
  }

protected:
  // basis topic ls
  TopicLsCommand topic_ls_command;
  // basis topic info
  TopicInfoCommand topic_info_command;
  // basis topic print
  TopicPrintCommand topic_print_command;
  // basis topic hz
  TopicHzCommand topic_hz_command;
};
} // namespace basis::cli