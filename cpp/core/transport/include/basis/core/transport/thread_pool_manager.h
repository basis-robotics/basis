#pragma once

#include <basis/core/threading/thread_pool.h>

namespace basis::core::transport {
/**
 * Simple class to manage thread pools across publishers.
 * Later this will be used to also manage named thread pools.
 */
class ThreadPoolManager {
public:
  ThreadPoolManager() = default;

  std::shared_ptr<threading::ThreadPool> GetDefaultThreadPool() { return default_thread_pool; }

private:
  std::shared_ptr<threading::ThreadPool> default_thread_pool = std::make_shared<threading::ThreadPool>(4);
};

}