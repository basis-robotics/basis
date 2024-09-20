
#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>

#include <basis/core/time.h>

namespace basis::core::containers {

class SubscriberOverallQueue {
public:

  std::optional<std::function<void()>> Pop(const Duration &sleep = basis::core::Duration::FromSecondsNanoseconds(0, 0)) {
    std::unique_lock lock(mutex);
    if (queue.empty()) {
      cv.wait_for(lock, std::chrono::duration<double>(sleep.ToSeconds()), [this] { return !queue.empty(); });
    }

    while (!queue.empty()) {
      auto front = queue.front();
      if (auto front_ptr = front.lock()) {
        queue.pop();
        return std::move(*front_ptr);
      }
      else {
        queue.pop();
      }
    }

    return std::nullopt;
  }

  size_t Size() const {
    std::lock_guard lock(mutex);
    return queue.size();
  }

  void AddCallback(const std::shared_ptr<std::function<void()>> &cb_ptr) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      queue.emplace(cb_ptr);
    }
    cv.notify_one();
  }

private:
  std::queue<std::weak_ptr<std::function<void()>>> queue; // Stores weak_ptrs to callbacks
  mutable std::mutex mutex;                                      // Mutex to protect the queue
  std::condition_variable cv;                            // Condition variable to signal when new callbacks are added
};

class SubscriberQueue {
public:
  SubscriberQueue(std::shared_ptr<SubscriberOverallQueue> overall_queue, size_t limit)
      : overall_queue(std::move(overall_queue)), limit(limit) {}

  // Set a new limit for this subscriber
  void SetLimit(size_t limit) {
    std::lock_guard<std::mutex> lock(mutex);
    this->limit = limit;
    EnforceLimit();
  }

  // Add a callback to the subscriber's queue
  void AddCallback(std::function<void()> callback) {
    auto cb_ptr = std::make_shared<std::function<void()>>(std::move(callback));

    {
      std::lock_guard<std::mutex> lock(mutex);
      callbacks.emplace_back(cb_ptr);
      EnforceLimit();
    }

    overall_queue->AddCallback(cb_ptr);
  }

private:
  void EnforceLimit() {
    if (limit == 0) {
      return;
    }

    while (callbacks.size() > limit) {
      callbacks.pop_front();
    }
  }

  std::shared_ptr<SubscriberOverallQueue> overall_queue;        // Shared pointer to the overall queue
  size_t limit;                                                 // Maximum number of callbacks allowed
  std::deque<std::shared_ptr<std::function<void()>>> callbacks; // Holds shared_ptrs to callbacks
  std::mutex mutex;                                             // Mutex to protect subscriber-specific data
};

using SubscriberQueueSharedPtr = std::shared_ptr<SubscriberQueue>;

} // namespace basis::core::containers
