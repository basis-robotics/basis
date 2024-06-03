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
    using namespace std::placeholders;

    time_test_pub = Advertise<TimeTest>("/time_test");

    // time_test_sub = Subscribe<TimeTest>("/time_test", std::function<void(std::shared_ptr<const
    // TimeTest>)>(std::bind(this, &ExampleUnit::OnTimeTest)));
    time_test_sub = Subscribe<TimeTest>("/time_test", std::bind(&ExampleUnit::OnTimeTest, this, _1));

#ifdef BASIS_ENABLE_ROS
    pc2_pub = Advertise<sensor_msgs::PointCloud2>("/point_cloud");
#endif
    rate_subscriber = std::make_unique<basis::core::transport::RateSubscriber>(
        basis::core::Duration::FromSecondsNanoseconds(1, 0),std::bind(&ExampleUnit::EveryOneSecond, this, _1));
  }

  void EveryOneSecond([[maybe_unused]] const basis::core::MonotonicTime time) {
    // Publish time message
    auto msg = std::make_shared<TimeTest>();
    msg->set_time(time.ToSeconds());
    spdlog::info("Publishing message [\n{}]", msg->DebugString());

    time_test_pub->Publish(msg);
#ifdef BASIS_ENABLE_ROS
    auto pc2_message = std::make_shared<sensor_msgs::PointCloud2>();
    pc2_message->header.stamp = ros::Time(time.ToSeconds());
    pc2_message->height = 2;
    pc2_message->width = 16;
    pc2_message->data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                         0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    pc2_pub->Publish(pc2_message);

    /*
        std::stringstream ss;
        ss << *pc2_message;
        spdlog::info("Publishing message [\n{}]", ss.str());
    */
    spdlog::info("Publishing ROS message");
#endif
  }

  void OnTimeTest(std::shared_ptr<const TimeTest> msg) { spdlog::info("Got message [\n{}]", msg->DebugString()); }
  std::shared_ptr<basis::core::transport::Publisher<TimeTest>> time_test_pub;
  std::shared_ptr<basis::core::transport::Subscriber<TimeTest>> time_test_sub;
#ifdef BASIS_ENABLE_ROS
  std::shared_ptr<basis::core::transport::Publisher<sensor_msgs::PointCloud2>> pc2_pub;

  std::unique_ptr<basis::core::transport::RateSubscriber> rate_subscriber;
#endif
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
  spdlog::cfg::load_env_levels();

  ExampleUnit example_unit;
  example_unit.Initialize();

  while (true) {
    example_unit.Update(1);
  }

  return 0;
}
