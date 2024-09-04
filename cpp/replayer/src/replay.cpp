#include "basis/replayer/config.h"
#include <basis/replayer/replay_args.h>
#include <argparse/argparse.hpp>
#include <basis/core/coordinator_connector.h>
#include <basis/core/transport/transport_manager.h>
#include <basis/replayer.h>
#include <filesystem>
#include <memory>

int main(int argc, char *argv[]) {
  using namespace basis::replayer;

  basis::core::logging::InitializeLoggingSystem();

  std::unique_ptr<argparse::ArgumentParser> parser = CreateArgumentParser();

  try {
    parser->parse_args(argc, argv);
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << parser;
    return 1;
  }

  std::unique_ptr<basis::core::transport::CoordinatorConnector> coordinator_connector = basis::core::transport::WaitForCoordinator();

  basis::core::transport::TransportManager transport_manager(nullptr);
  transport_manager.RegisterTransport("net_tcp", std::make_unique<basis::plugins::transport::TcpTransport>());

  Config config;
  config.loop = parser->get<bool>(LOOP_ARG);
  config.input = parser->get(RECORDING_ARG);

  basis::Replayer replayer(std::move(config), transport_manager, *coordinator_connector);

  replayer.Run();

  return 0;
}