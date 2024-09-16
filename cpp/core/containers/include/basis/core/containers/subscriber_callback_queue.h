
#pragma once
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream> // TODO for debugging only
#include <functional>
#include <list>
#include <memory>
#include <mutex>

#include <basis/core/time.h>

namespace basis::core::containers {

class SubscriberOverallQueue {
public:
  // Add a callback to the overall queue
  void AddCallback(const std::shared_ptr<std::function<void()>> &cb_ptr) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      queue.emplace_back(cb_ptr); // Store as weak_ptr
    }
    cv.notify_one(); // Notify that a new callback is available
  }

  // Process callbacks, waiting for new items or until timeout
  void ProcessCallbacks(const basis::core::Duration &max_sleep_duration) {
    std::unique_lock<std::mutex> lock(mutex);
  
    // Wait until notified or timeout
    cv.wait_for(lock, std::chrono::duration<double>(max_sleep_duration.ToSeconds()), [this]() { return !queue.empty(); });

    if (queue.empty()) {
      // No callbacks to process, return
      return;
    }

    // Swap the queue with a local one
    std::list<std::weak_ptr<std::function<void()>>> callbacks_to_process;
    callbacks_to_process.swap(queue);
    lock.unlock();

    // Process callbacks outside the lock
    for (auto &weak_cb_ptr : callbacks_to_process) {
      if (auto cb_ptr = weak_cb_ptr.lock()) {
        // Callback is still valid
        (*cb_ptr)();
      }
    }
  }

private:
  std::list<std::weak_ptr<std::function<void()>>> queue; // Stores weak_ptrs to callbacks
  std::mutex mutex;                                      // Mutex to protect the queue
  std::condition_variable cv;                            // Condition variable to signal when new callbacks are added
};

class SubscriberQueue {
public:
  // Constructor: associates with the SubscriberOverallQueue and sets the limit
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

    overall_queue->AddCallback(cb_ptr); // Pass shared_ptr to SubscriberOverallQueue
  }

private:
  void EnforceLimit() {
    if (limit == 0) {
      return;
    }

    if (callbacks.size() > limit) {
      std::cerr << "SubscriberQueue limit reached " << callbacks.size() << " --> " << limit << std::endl;
    }
    while (callbacks.size() > limit) {
      // Remove the oldest callback
      callbacks.pop_front(); // shared_ptr destroyed here, possibly destroying the callback
    }
  }

  std::shared_ptr<SubscriberOverallQueue> overall_queue;        // Shared pointer to the overall queue
  size_t limit;                                                 // Maximum number of callbacks allowed
  std::deque<std::shared_ptr<std::function<void()>>> callbacks; // Holds shared_ptrs to callbacks
  std::mutex mutex;                                             // Mutex to protect subscriber-specific data
};

using SubscriberQueueSharedPtr = std::shared_ptr<SubscriberQueue>;

}