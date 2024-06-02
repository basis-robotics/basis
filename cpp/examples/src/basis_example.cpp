/**
 * @file basis_example.cpp
 *
 * A living example of how to use basis, that should have any new features added to it as they are implemented.
 *
 */
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

#include <basis/core/coordinator_connector.h>
#include <basis/core/transport/transport_manager.h>

#include <test.pb.h>

#ifdef BASIS_ENABLE_ROS
#include <basis/plugins/serialization/rosmsg.h>
#include <sensor_msgs/PointCloud2.h>
#endif

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
  spdlog::cfg::load_env_levels();

  // todo: add wait for connection option to connector?
  auto connector = basis::core::transport::CoordinatorConnector::Create();

  if (!connector) {
    spdlog::warn("No connection to the coordinator, running without coordinator");
  }

  // todo why not output queue

  auto thread_pool_manager = std::make_shared<basis::core::transport::ThreadPoolManager>();

  basis::core::transport::TransportManager transport_manager(
      std::make_unique<basis::core::transport::InprocTransport>());
  transport_manager.RegisterTransport("net_tcp",
                                      std::make_unique<basis::plugins::transport::TcpTransport>(thread_pool_manager));

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /// @todo BASIS-18 when running without coordinator and without inproc, this exact order of advertise, update,
  /// subscribe is required
  auto time_test_pub = transport_manager.Advertise<TimeTest>("/time_test");

  transport_manager.Update();

  auto time_test_sub = transport_manager.Subscribe<TimeTest>(
      "/time_test", [](auto msg) { spdlog::info("Got message [\n{}]", msg->DebugString()); });

#ifdef BASIS_ENABLE_ROS
  auto pc2_pub = transport_manager.Advertise<sensor_msgs::PointCloud2>("/point_cloud");
#endif

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  while (true) {
    spdlog::info("Example Tick");
    // let the transport manager gather any new publishers
    transport_manager.Update();
    // send it off to the coordinator
    if (connector) {
      std::vector<basis::core::serialization::MessageSchema> new_schemas =
          transport_manager.GetSchemaManager().ConsumeSchemasToSend();
      if (new_schemas.size()) {
        connector->SendSchemas(new_schemas);
      }
      connector->SendTransportManagerInfo(transport_manager.GetTransportManagerInfo());
      connector->Update();

      if (connector->GetLastNetworkInfo()) {
        transport_manager.HandleNetworkInfo(*connector->GetLastNetworkInfo());
      }
    }
    const auto current_time = std::chrono::system_clock::now();
    const auto duration_in_seconds = std::chrono::duration<double>(current_time.time_since_epoch());
    const double num_seconds = duration_in_seconds.count();

    // Publish time message
    auto msg = std::make_shared<TimeTest>();
    msg->set_time(num_seconds);
    spdlog::info("Publishing message [\n{}]", msg->DebugString());

    time_test_pub->Publish(msg);
#ifdef BASIS_ENABLE_ROS
    auto pc2_message = std::make_shared<sensor_msgs::PointCloud2>();
    pc2_message->header.stamp = ros::Time(num_seconds);
    pc2_message->height = 2;
    pc2_message->width = 16;
    pc2_message->data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                         0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    pc2_pub->Publish(pc2_message);

    std::stringstream ss;
    ss << *pc2_message;
    spdlog::info("Publishing message [\n{}]", ss.str());

#endif
    spdlog::info("Sleep ~1 second", msg->DebugString());
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return 0;
}
