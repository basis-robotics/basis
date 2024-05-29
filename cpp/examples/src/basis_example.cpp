/**
 * @file basis_example.cpp
 * 
 * A living example of how to use basis, that should have any new features added to it as they are implemented.
 * 
 */
#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>

#include <basis/core/coordinator_connector.h>
#include <basis/core/transport/transport_manager.h>

#include <test.pb.h>

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
  spdlog::cfg::load_env_levels();

  // todo: add wait for connection option to connector?
  auto connector = basis::core::transport::CoordinatorConnector::Create();

  if(!connector) {
    spdlog::warn("No connection to the coordinator, running without coordinator");
  }

  auto thread_pool_manager = std::make_shared<basis::core::transport::ThreadPoolManager>();

  basis::core::transport::TransportManager transport_manager(std::make_unique<basis::core::transport::InprocTransport>());
  transport_manager.RegisterTransport("net_tcp",
                                      std::make_unique<basis::plugins::transport::TcpTransport>(thread_pool_manager));


  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @todo BASIS-18 when running without coordinator and without inproc, this exact order of advertise, update, subscribe is required

  // TODO: add ROS example
  auto time_test_pub = transport_manager.Advertise<TimeTest>("/time_test");
  
  transport_manager.Update();

  auto time_test_sub = transport_manager.Subscribe<TimeTest>("/time_test", [](auto msg){
    spdlog::info("Got message [\n{}]", msg->DebugString());
  });
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  while (true) {
    spdlog::info("Example Tick");
    // let the transport manager gather any new publishers
    transport_manager.Update();
    // send it off to the coordinator
    if(connector) {
      connector->SendTransportManagerInfo(transport_manager.GetTransportManagerInfo());
      connector->Update();

      if (connector->GetLastNetworkInfo()) {
        transport_manager.HandleNetworkInfo(*connector->GetLastNetworkInfo());
      }
    }
    auto current_time = std::chrono::system_clock::now();
    auto duration_in_seconds = std::chrono::duration<double>(current_time.time_since_epoch());
    double num_seconds = duration_in_seconds.count();

    auto msg = std::make_shared<TimeTest>();
    msg->set_time(num_seconds);
    spdlog::info("Publishing message [\n{}]", msg->DebugString());

    time_test_pub->Publish(msg);
    spdlog::info("Sleep ~1 second", msg->DebugString());
    std::this_thread::sleep_for(std::chrono::seconds(1));

  }
  return 0;
}
