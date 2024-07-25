#include "basis/replayer/config.h"
#include <argparse/argparse.hpp>
#include <basis/core/coordinator_connector.h>
#include <basis/core/transport/transport_manager.h>
#include <basis/replayer.h>
#include <filesystem>
#include <memory>

int main(int argc, char *argv[]) {
  using namespace basis::replayer;

  basis::core::logging::InitializeLoggingSystem();

  argparse::ArgumentParser parser("replay");

  constexpr char LOOP_ARG[] = "--loop";
  parser.add_argument(LOOP_ARG).help("Whether or not to loop the playback.").default_value(false).implicit_value(true);
  // TODO: allow multiple mcap files, directory support
  constexpr char RECORDING_ARG[] = "recording";
  parser.add_argument(RECORDING_ARG).help("The MCAP file to replay.");

  try {
    parser.parse_args(argc, argv);
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << parser;
    return 1;
  }

  std::unique_ptr<basis::core::transport::CoordinatorConnector> coordinator_connector;
  while (!coordinator_connector) {
    coordinator_connector = basis::core::transport::CoordinatorConnector::Create();
    if (!coordinator_connector) {
      BASIS_LOG_WARN("No connection to the coordinator, waiting 1 second and trying again");
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  basis::core::transport::TransportManager transport_manager(nullptr);
  transport_manager.RegisterTransport("net_tcp", std::make_unique<basis::plugins::transport::TcpTransport>());

  Config config;
  config.loop = parser.get<bool>(LOOP_ARG);
  config.input = parser.get(RECORDING_ARG);

  basis::Replayer replayer(std::move(config), transport_manager, *coordinator_connector);

  replayer.Run();

  return 0;
}