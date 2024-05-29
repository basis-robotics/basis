#include <spdlog/spdlog.h>

#include <basis/core/coordinator_connector.h>
#include <basis/core/transport/transport.h>

#include <test.pb.h>

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
  auto connector = basis::core::transport::CoordinatorConnector::Create();

  auto thread_pool_manager = std::make_shared<basis::core::transport::ThreadPoolManager>();
  basis::core::transport::TransportManager transport_manager;
  transport_manager.RegisterTransport("net_tcp",
                                      std::make_unique<basis::plugins::transport::TcpTransport>(thread_pool_manager));

  // auto publish_proto_
  auto time_test_pub = transport_manager.Advertise<TimeTest>("/time_test");

  while (true) {
    spdlog::info("Example Tick");
    // let the transport manager gather any new publishers
    transport_manager.Update();
    // send it off to the coordinator
    connector->SendTransportManagerInfo(transport_manager.GetTransportManagerInfo());
    connector->Update();

    if (connector->GetLastNetworkInfo()) {
      transport_manager.HandleNetworkInfo(*connector->GetLastNetworkInfo());
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return 0;
}
