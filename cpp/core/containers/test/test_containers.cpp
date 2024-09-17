#include <basis/core/containers/subscriber_callback_queue.h>

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace containers = basis::core::containers;

// Mock class to capture callback invocations
class CallbackMock {
public:
  void Callback(int id) {
    std::lock_guard<std::mutex> lock(mutex);
    called_ids.push_back(id);
  }

  std::vector<int> GetCalledIds() {
    std::lock_guard<std::mutex> lock(mutex);
    return called_ids;
  }

  void Clear() {
    std::lock_guard<std::mutex> lock(mutex);
    called_ids.clear();
  }

private:
  std::vector<int> called_ids;
  std::mutex mutex;
};

// Test fixture for SubscriberQueue
class SubscriberQueueTest : public ::testing::Test {
protected:
  void SetUp() override {
    overall_queue = std::make_shared<containers::SubscriberOverallQueue>();
    callback_mock = std::make_shared<CallbackMock>();
  }

  std::shared_ptr<containers::SubscriberOverallQueue> overall_queue;
  std::shared_ptr<CallbackMock> callback_mock;
};

// Helper function to process all available callbacks
void ProcessAllCallbacks(std::shared_ptr<containers::SubscriberOverallQueue> overall_queue) {
  while (auto event = overall_queue->Pop()) {
    (*event)();
  }
}

TEST_F(SubscriberQueueTest, SingleCallbackProcessed) {
  // Create a SubscriberQueue with a limit
  containers::SubscriberQueue subscriber(overall_queue, 10);

  // Add a callback
  subscriber.AddCallback([this]() { callback_mock->Callback(1); });

  // Process the callbacks
  ProcessAllCallbacks(overall_queue);

  // Check that the callback was called
  auto called_ids = callback_mock->GetCalledIds();
  ASSERT_EQ(called_ids.size(), 1);
  EXPECT_EQ(called_ids[0], 1);
}

TEST_F(SubscriberQueueTest, NoLimit) {
  // Create a SubscriberQueue with a limit
  containers::SubscriberQueue subscriber(overall_queue, 0);

  // Add a callback
  subscriber.AddCallback([this]() { callback_mock->Callback(1); });
  subscriber.AddCallback([this]() { callback_mock->Callback(2); });

  // Process the callbacks
  ProcessAllCallbacks(overall_queue);

  // Check that the callback was called
  auto called_ids = callback_mock->GetCalledIds();
  ASSERT_EQ(called_ids.size(), 2);
  EXPECT_EQ(called_ids[0], 1);
  EXPECT_EQ(called_ids[1], 2);
}

TEST_F(SubscriberQueueTest, LimitEnforced) {
  // Create a SubscriberQueue with a limit of 2
  containers::SubscriberQueue subscriber(overall_queue, 2);

  // Add three callbacks
  subscriber.AddCallback([this]() { callback_mock->Callback(1); }); // Oldest callback should be discarded
  subscriber.AddCallback([this]() { callback_mock->Callback(2); });
  subscriber.AddCallback([this]() { callback_mock->Callback(3); });

  // Process the callbacks
  ProcessAllCallbacks(overall_queue);

  // Check that only callbacks 2 and 3 were called
  auto called_ids = callback_mock->GetCalledIds();
  ASSERT_EQ(called_ids.size(), 2);
  EXPECT_EQ(called_ids[0], 2);
  EXPECT_EQ(called_ids[1], 3);
}

TEST_F(SubscriberQueueTest, MultipleSubscribersProcessedCorrectly) {
  // Create multiple subscribers
  containers::SubscriberQueue subscriber1(overall_queue, 2);
  containers::SubscriberQueue subscriber2(overall_queue, 3);

  // Add callbacks to subscriber1
  subscriber1.AddCallback([this]() { callback_mock->Callback(1); });
  subscriber1.AddCallback([this]() { callback_mock->Callback(2); });
  subscriber1.AddCallback([this]() { callback_mock->Callback(3); }); // Oldest callback discarded

  // Add callbacks to subscriber2
  subscriber2.AddCallback([this]() { callback_mock->Callback(4); });
  subscriber2.AddCallback([this]() { callback_mock->Callback(5); });

  // Process the callbacks
  ProcessAllCallbacks(overall_queue);

  // Check that callbacks 2, 3, 4, and 5 were called
  auto called_ids = callback_mock->GetCalledIds();
  ASSERT_EQ(called_ids.size(), 4);
  EXPECT_EQ(called_ids[0], 2);
  EXPECT_EQ(called_ids[1], 3);
  EXPECT_EQ(called_ids[2], 4);
  EXPECT_EQ(called_ids[3], 5);
}

TEST_F(SubscriberQueueTest, PopTimeoutReturnsEmptyOptional) {
  // Pop from an empty queue with zero sleep duration
  auto callback_opt = overall_queue->Pop(basis::core::Duration::FromSecondsNanoseconds(0, 0));
  // Should return an empty optional
  ASSERT_FALSE(callback_opt.has_value());
}

TEST_F(SubscriberQueueTest, ConcurrentAccess) {
  // Create a SubscriberQueue with a large limit
  containers::SubscriberQueue subscriber(overall_queue, 1000);

  // Create multiple threads to add callbacks
  const int num_threads = 10;
  const int callbacks_per_thread = 50;
  std::vector<std::thread> threads;

  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([this, &subscriber, t]() {
      for (int i = 0; i < callbacks_per_thread; ++i) {
        int id = t * callbacks_per_thread + i;
        subscriber.AddCallback([this, id]() { callback_mock->Callback(id); });
      }
    });
  }

  // Wait for all threads to finish
  for (auto &thread : threads) {
    thread.join();
  }

  // Process the callbacks
  ProcessAllCallbacks(overall_queue);

  // Check that all callbacks were called
  auto called_ids = callback_mock->GetCalledIds();
  ASSERT_EQ(called_ids.size(), num_threads * callbacks_per_thread);

  // Sort the called IDs to verify all IDs are present
  std::sort(called_ids.begin(), called_ids.end());
  for (int i = 0; i < num_threads * callbacks_per_thread; ++i) {
    EXPECT_EQ(called_ids[i], i);
  }
}

TEST_F(SubscriberQueueTest, SetLimitEnforcesNewLimit) {
  // Create a SubscriberQueue with an initial limit of 5
  containers::SubscriberQueue subscriber(overall_queue, 5);

  // Add five callbacks
  for (int i = 1; i <= 5; ++i) {
    subscriber.AddCallback([this, i]() { callback_mock->Callback(i); });
  }

  // Change the limit to 3
  subscriber.SetLimit(3);

  // Process the callbacks
  ProcessAllCallbacks(overall_queue);

  // Check that only the last three callbacks were called
  auto called_ids = callback_mock->GetCalledIds();
  ASSERT_EQ(called_ids.size(), 3);
  EXPECT_EQ(called_ids[0], 3);
  EXPECT_EQ(called_ids[1], 4);
  EXPECT_EQ(called_ids[2], 5);
}

TEST_F(SubscriberQueueTest, ZeroLimit) {
  // Create a SubscriberQueue with a limit of 0
  containers::SubscriberQueue subscriber(overall_queue, 0);

  // Add callbacks
  subscriber.AddCallback([this]() { callback_mock->Callback(1); });
  subscriber.AddCallback([this]() { callback_mock->Callback(2); });

  // Process the callbacks
  ProcessAllCallbacks(overall_queue);

  // Check that no callbacks were called
  auto called_ids = callback_mock->GetCalledIds();
  ASSERT_EQ(called_ids.size(), 2);
}
