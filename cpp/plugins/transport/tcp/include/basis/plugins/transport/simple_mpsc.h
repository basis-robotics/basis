#pragma once

#include <mutex>
#include <queue>

namespace basis::plugins::transport {

/**
 * Simple thread safe Multi-producer Single Consumer Queue
 * Used as a placeholder for now.
 * In theory, safe for MPMC as well - needs testing.
 */
template <typename T>
class SimpleMPSCQueue {
public:
  SimpleMPSCQueue() = default;
  /**
   * Adds item to the queue
   */
  void Emplace(T &&item) {
    {
      std::lock_guard lock(queue_mutex);
      queue.emplace(item);
    }
    queue_cv.notify_one();
  }

  size_t Size() {
    std::lock_guard lock(queue_mutex);
    return queue.size();
  }

  /**
   * Removes an item out of the queue - or nothing if there's a timeout.
   * @todo I don't see why this can't be used as MPMC
   */
  std::optional<T> Pop(int sleep_s) {
    std::unique_lock lock(queue_mutex);
    if (queue.empty()) {
      queue_cv.wait_for(lock, std::chrono::seconds(sleep_s), [this] { return !queue.empty(); });
    }
    std::optional<T> ret;
    if (!queue.empty()) {
      ret = std::move(queue.front());
      queue.pop();
    }
    return ret;
  }

  std::mutex queue_mutex;
  std::condition_variable queue_cv;
  std::queue<T> queue;

};

}