#include <basis/core/transport/inproc.h>
#include <basis/core/transport/transport_manager.h>

#include <gtest/gtest.h>
#include <thread>
using namespace basis::core::transport;

TEST(Inproc, PubSub) {
  // Create a Coordinator
  InprocConnector<int> coordinator;
  // Create a publisher
  auto publisher = coordinator.Advertise("topic");

  std::atomic<int> num_recv = 0;

  auto subscriber = coordinator.Subscribe(
      "topic", [&num_recv](const MessageEvent<int> &message) { GTEST_ASSERT_EQ(*message.message, num_recv++); });

  for (int i = 0; i < 10; i++) {
    publisher->Publish(std::make_shared<int>(i));
  }

  GTEST_ASSERT_EQ(num_recv, 10);
}

TEST(Inproc, PubSubWait) {
  // Create a Coordinator
  InprocConnector<int> coordinator;
  // Create a publisher
  auto publisher = coordinator.Advertise("topic");

  std::atomic<int> num_recv = 0;

  auto subscriber = coordinator.Subscribe(
      "topic", [&num_recv](const MessageEvent<int> &message) { ASSERT_EQ(*message.message, num_recv++); });

  std::thread pub_thread([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    for (int i = 0; i < 10; i++) {
      publisher->Publish(std::make_shared<int>(i));
    }
  });

  pub_thread.join();

  GTEST_ASSERT_EQ(num_recv, 10);
}

struct TestStruct {
  uint32_t foo = 3;
  float bar = 8.5;
  std::string baz = "baz";
};

TEST(TransportManager, Basic) {
  auto thread_pool = std::make_shared<ThreadPoolManager>();

  TransportManager transport_manager(std::make_unique<InprocTransport>());

  auto publisher =
      transport_manager.Advertise<TestStruct, basis::core::serialization::RawSerializer>("InprocTransport");

 basis::core::threading::ThreadPool work_thread_pool(4);

  std::atomic<int> num_recv = 0;
  auto subscriber = transport_manager.Subscribe<TestStruct, basis::core::serialization::RawSerializer>(
      "InprocTransport", [&num_recv](std::shared_ptr<const TestStruct>) { num_recv++; }, &work_thread_pool);

  publisher->Publish(std::make_shared<TestStruct>());
  ASSERT_EQ(num_recv, 1);
}