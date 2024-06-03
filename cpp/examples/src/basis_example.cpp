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

#include <basis/unit.h>

class ExampleUnit : public basis::SingleThreadedUnit {
public:
  void Initialize() {
    time_test_pub = Advertise<TimeTest>("/time_test");

    time_test_sub =
        Subscribe<TimeTest>("/time_test", [](auto msg) { spdlog::info("Got message [\n{}]", msg->DebugString()); });

#ifdef BASIS_ENABLE_ROS
    pc2_pub = Advertise<sensor_msgs::PointCloud2>("/point_cloud");
#endif
  }
  std::shared_ptr<basis::core::transport::Publisher<TimeTest>> time_test_pub;
  std::shared_ptr<basis::core::transport::Subscriber<TimeTest>> time_test_sub;
#ifdef BASIS_ENABLE_ROS
  std::shared_ptr<basis::core::transport::Publisher<sensor_msgs::PointCloud2>> pc2_pub;
#endif
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
  spdlog::cfg::load_env_levels();

  ExampleUnit example_unit;
  example_unit.Initialize();

  while (true) {
    example_unit.Update();

    // TODO: need to have a way of marking up nodes to have a fixed update

    const auto current_time = std::chrono::system_clock::now();
    const auto duration_in_seconds = std::chrono::duration<double>(current_time.time_since_epoch());
    const double num_seconds = duration_in_seconds.count();

    // Publish time message
    auto msg = std::make_shared<TimeTest>();
    msg->set_time(num_seconds);
    spdlog::info("Publishing message [\n{}]", msg->DebugString());

    example_unit.time_test_pub->Publish(msg);
#ifdef BASIS_ENABLE_ROS
    auto pc2_message = std::make_shared<sensor_msgs::PointCloud2>();
    pc2_message->header.stamp = ros::Time(num_seconds);
    pc2_message->height = 2;
    pc2_message->width = 16;
    pc2_message->data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                         0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    example_unit.pc2_pub->Publish(pc2_message);

    std::stringstream ss;
    ss << *pc2_message;
    spdlog::info("Publishing message [\n{}]", ss.str());

#endif
    spdlog::info("Sleep ~1 second", msg->DebugString());
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return 0;
}
