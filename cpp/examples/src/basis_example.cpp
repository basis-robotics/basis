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

#include <basis/synchronizers/field.h>

// Plugins
#include <basis/plugins/serialization/protobuf.h>
#ifdef BASIS_ENABLE_ROS
#include <basis/plugins/serialization/rosmsg.h>
#endif

// Message definitions
#include <basis_example.pb.h>
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
  ExampleUnit() : basis::SingleThreadedUnit("ExampleUnit") {}

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

    stamped_vector_pub = Advertise<ExampleStampedVector>("/stamped_vector");

    // Subscribe to changes in time, at a rate of 1Hz
    one_hz_rate_subscriber = std::make_unique<basis::core::transport::RateSubscriber>(
        basis::core::Duration::FromSecondsNanoseconds(1, 0), std::bind(&ExampleUnit::EveryOneSecond, this, _1));

    ten_hz_rate_subscriber = std::make_unique<basis::core::transport::RateSubscriber>(
        basis::core::Duration::FromSecondsNanoseconds(0, std::nano::den / 10),
        std::bind(&ExampleUnit::EveryTenHertz, this, _1));

#ifdef BASIS_ENABLE_ROS
    vector_lidar_sync = std::make_unique<decltype(vector_lidar_sync)::element_type>(
        std::bind(&ExampleUnit::OnSyncedVectorLidar, this, _1, _2));

    vector_sub =
        Subscribe<ExampleStampedVector>("/stamped_vector", [this](auto m) { vector_lidar_sync->OnMessage<0>(m); });

    pc2_sub =
        Subscribe<sensor_msgs::PointCloud2>("/point_cloud", [this](auto m) { vector_lidar_sync->OnMessage<1>(m); });

    sync_event_pub = Advertise<SyncedVectorLidarEvent>("/synced_vector_lidar");
#endif
  }

protected:
  /**
   * This handler will be called once per second (see Initialize()).
   *
   * Inside, we will publish our time message at a regular rate.
   *
   * @param time the current monotonic time
   */
  void EveryOneSecond(const basis::core::MonotonicTime time) {
    // Create a protobuf message
    auto msg = std::make_shared<TimeTest>();
    // Set a member of it
    msg->set_time(time.ToSeconds());
    // Publish
    time_test_pub->Publish(msg);
    BASIS_LOG_INFO("Published message [\n{}]", msg->DebugString());

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
    BASIS_LOG_INFO("Published ROS message");
#endif
  }

  void EveryTenHertz(const basis::core::MonotonicTime time) {
    const double time_sec = time.ToSeconds();
    auto stamped_v = std::make_shared<ExampleStampedVector>();
    stamped_v->set_time(time_sec);
    auto *pos = stamped_v->mutable_pos();
    pos->set_x(time_sec / 100.0);
    pos->set_y(sin(time_sec / 100.0));
    pos->set_z(0);
    stamped_vector_pub->Publish(std::move(stamped_v));
  }

#ifdef BASIS_ENABLE_ROS
  void OnSyncedVectorLidar(std::shared_ptr<const ExampleStampedVector> v,
                           std::shared_ptr<const sensor_msgs::PointCloud2> p) {
    auto msg = std::make_shared<SyncedVectorLidarEvent>();
    msg->set_time_vector(v->time());
    msg->set_time_lidar(p->header.stamp.toSec());
    sync_event_pub->Publish(msg);
  }
#endif
  /**
   * Called whenever a message is received over /time_test
   * @param msg
   */
  void OnTimeTest(std::shared_ptr<const TimeTest> msg) {
    auto now = basis::core::MonotonicTime::Now();
    // Note that latency numbers will vary depending on the transport
    BASIS_LOG_INFO("Got message with {:f}ms latency\n[\n{}]", ((now.ToSeconds() - msg->time()) * 1000),
                   msg->DebugString());
    time_test_pub_forwarded->Publish(msg);
  }

  // Our subscribers
  std::shared_ptr<basis::core::transport::Subscriber<TimeTest>> time_test_sub;
#ifdef BASIS_ENABLE_ROS
  std::shared_ptr<basis::core::transport::Subscriber<ExampleStampedVector>> vector_sub;
  std::shared_ptr<basis::core::transport::Subscriber<sensor_msgs::PointCloud2>> pc2_sub;
#endif
  std::unique_ptr<basis::core::transport::RateSubscriber> one_hz_rate_subscriber;
  std::unique_ptr<basis::core::transport::RateSubscriber> ten_hz_rate_subscriber;

  // Our publishers
  std::shared_ptr<basis::core::transport::Publisher<TimeTest>> time_test_pub;
  std::shared_ptr<basis::core::transport::Publisher<TimeTest>> time_test_pub_forwarded;
#ifdef BASIS_ENABLE_ROS
  std::shared_ptr<basis::core::transport::Publisher<sensor_msgs::PointCloud2>> pc2_pub;
  std::shared_ptr<basis::core::transport::Publisher<SyncedVectorLidarEvent>> sync_event_pub;
#endif
  std::shared_ptr<basis::core::transport::Publisher<ExampleStampedVector>> stamped_vector_pub;

#ifdef BASIS_ENABLE_ROS
  // Synchronize together lidar and vector data
  // lidar is 1hz here, and the vector is 10hz
  // they won't always line up exactly, so be liberal about matching them together
  std::unique_ptr<basis::synchronizers::FieldSyncApproximatelyEqual<
      0.05, basis::synchronizers::Field<std::shared_ptr<const ExampleStampedVector>, &ExampleStampedVector::time>,
      basis::synchronizers::Field<std::shared_ptr<const sensor_msgs::PointCloud2>,
                                  [](const sensor_msgs::PointCloud2 *msg) { return msg->header.stamp.toSec(); }>>>
      vector_lidar_sync;
#endif
};

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
  basis::core::logging::InitializeLoggingSystem();

  ExampleUnit example_unit;
  example_unit.WaitForCoordinatorConnection();
  example_unit.CreateTransportManager();
  example_unit.Initialize();

  while (true) {
    example_unit.Update(basis::core::Duration::FromSecondsNanoseconds(1, 0));
  }

  return 0;
}
