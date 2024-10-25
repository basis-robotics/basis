#pragma once

#include <mutex>
#include <optional>
#include <queue>

#include <basis/core/time.h>

namespace basis::core::containers {

/**
 * Simple thread safe Multi-producer Single Consumer Queue
 * Used as a placeholder for now.
 * In theory, safe for MPMC as well - needs testing.
 *
 * TODO: consider using spdlog::details::mpmc_blocking_q
 */
template <typename T> class SimpleMPSCQueue {
public:
  SimpleMPSCQueue() = default;
  /**
   * Adds item to the queue
   */
  void Emplace(T &&item) {
    {
      std::lock_guard lock(queue_mutex);
      queue.emplace(std::move(item));
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
   * @todo It would be great if the mspc queue could take an std::atomic bool for when to stop
   */
  std::optional<T> Pop(const Duration &sleep = basis::core::Duration::FromSecondsNanoseconds(0, 0)) {
    std::unique_lock lock(queue_mutex);
    if (queue.empty()) {
      queue_cv.wait_for(lock, std::chrono::duration<double>(sleep.ToSeconds()), [this] { return !queue.empty(); });
    }
    std::optional<T> ret;
    if (!queue.empty()) {
      ret = std::move(queue.front());
      queue.pop();
    }
    return ret;
  }

  // todo: add PopAll()

  std::mutex queue_mutex;
  std::condition_variable queue_cv;
  std::queue<T> queue;
};

} // namespace basis::core::containers