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

void PrintTopic(std::string_view topic, basis::core::transport::CoordinatorConnector *connector,
                std::optional<size_t> max_num_messages) {
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
                                                        spdlog::info("Got message number {} of size {}", (size_t)num_messages, msg->GetPayload().size());
                                                        spdlog::info("print functionality requires schema support over network");
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
  // basis topic ls
  argparse::ArgumentParser topic_info_command("info");
  topic_info_command.add_description("get a topic's information");
  topic_info_command.add_argument("topic");
  topic_command.add_subparser(topic_info_command);
  // basis topic print
  argparse::ArgumentParser topic_print_command("print");
  topic_print_command.add_description("print a message on the topic");
  topic_print_command.add_argument("topic");
  topic_print_command.add_argument("-n").scan<'i', size_t>().help("number of messages to print (default: infinite)");
  topic_command.add_subparser(topic_print_command);

  parser.add_subparser(topic_command);

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
        std::cout << "  " << topic << " (" << pubs.publishers().size() << ")" << std::endl;
      }
    } else if (topic_command.is_subcommand_used("info")) {
      auto topic = topic_info_command.get("topic");

      auto it = info->publishers_by_topic().find(topic);
      if (it == info->publishers_by_topic().end()) {
        std::cout << "No publishers for topic " << topic << std::endl;
        return 1;
      }
      std::cout << "topic: " << topic << std::endl;
      std::cout << "type: <TODO>" << std::endl;
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
      PrintTopic(topic, connection.get(), topic_print_command.present<size_t>("-n"));
    }
  }

  return 0;
}
