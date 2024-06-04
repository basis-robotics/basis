/**
 * @file basis_example.cpp
 *
 * A living example of how to use basis, that should have any new features added to it as they are implemented.
 *
 */

// Logging
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

// Core
#include <basis/unit.h>

// Plugins
#include <basis/plugins/serialization/protobuf.h>
#ifdef BASIS_ENABLE_ROS
#include <basis/plugins/serialization/rosmsg.h>
#endif

// Message definitions
#include <test.pb.h>
#ifdef BASIS_ENABLE_ROS
#include <sensor_msgs/PointCloud2.h>
#endif

/**
 * An example Unit class.
 *
 * Single threaded, multiple callbacks can be added without having to write your own multithreading guards.
 */
class ExampleUnit : public basis::SingleThreadedUnit {
public:
  void Initialize() {
    using namespace std::placeholders;


    // Subscribe to the /time_test topic, with a protobuf message TimeTest.
    time_test_sub = Subscribe<TimeTest>("/time_test", std::bind(&ExampleUnit::OnTimeTest, this, _1));

    // Advertise to that same topic - we will get our own subscriptions (via inproc), and any others from the network.    
    time_test_pub = Advertise<TimeTest>("/time_test");
    // Advertise to another topic, to demonstrate publishing from a subscription
    time_test_pub_forwarded = Advertise<TimeTest>("/time_test_forwarded");

    // (Optionally) Advertise a ROS message, too.
#ifdef BASIS_ENABLE_ROS
    pc2_pub = Advertise<sensor_msgs::PointCloud2>("/point_cloud");
#endif

    // Subscribe to changes in time, at a rate of 1Hz
    rate_subscriber = std::make_unique<basis::core::transport::RateSubscriber>(
        basis::core::Duration::FromSecondsNanoseconds(1, 0), std::bind(&ExampleUnit::EveryOneSecond, this, _1));
  }

protected:
  /**
   * This handler will be called once per second (see Initialize()).
   *
   * Inside, we will publish our time message at a regular rate.
   *
   * @param time the current monotonic time
   */
  void EveryOneSecond([[maybe_unused]] const basis::core::MonotonicTime time) {
    // Create a protobuf message
    auto msg = std::make_shared<TimeTest>();
    // Set a member of it
    msg->set_time(time.ToSeconds());
    // Publish
    time_test_pub->Publish(msg);
    spdlog::info("Published message [\n{}]", msg->DebugString());

#ifdef BASIS_ENABLE_ROS
    // TODO: move this out to an ExternalCallback, to simulate a sensor

    // Create a ROS message
    auto pc2_message = std::make_shared<sensor_msgs::PointCloud2>();
    // Populate it with a real time (TODO: ros times are based on system clock, we should consider doing that
    // conversion)
    pc2_message->header.stamp = ros::Time(time.ToSeconds());
    // And some dummy data
    pc2_message->height = 2;
    pc2_message->width = 16;
    pc2_message->data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                         0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    // Publish it
    pc2_pub->Publish(pc2_message);
    spdlog::info("Published ROS message");
#endif
  }

  /**
   * Called whenever a message is received over /time_test
   * @param msg
   */
  void OnTimeTest(std::shared_ptr<const TimeTest> msg) {
    auto now = basis::core::MonotonicTime::Now();
    // Note that latency numbers will vary depending on the transport
    spdlog::info("Got message with {:f}ms latency\n[\n{}]", ((now.ToSeconds() - msg->time()) * 1000),
                 msg->DebugString());
    time_test_pub_forwarded->Publish(msg);
  }

  // Our subscribers
  std::shared_ptr<basis::core::transport::Subscriber<TimeTest>> time_test_sub;
  std::unique_ptr<basis::core::transport::RateSubscriber> rate_subscriber;

  // Our publishers
  std::shared_ptr<basis::core::transport::Publisher<TimeTest>> time_test_pub;
  std::shared_ptr<basis::core::transport::Publisher<TimeTest>> time_test_pub_forwarded;
#ifdef BASIS_ENABLE_ROS
  std::shared_ptr<basis::core::transport::Publisher<sensor_msgs::PointCloud2>> pc2_pub;
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
