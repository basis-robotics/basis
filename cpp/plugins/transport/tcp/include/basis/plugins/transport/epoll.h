#pragma once
/**
 * First try at an epoll interface. This is destined to live in a helper library - Unix Domain Socket transports will
 * need the same thing. It's also possible this will be replaced completely with something libuv based.
 */

#include <cassert>
#include <fcntl.h>
#include <spdlog/spdlog.h>
#include <sys/epoll.h>

namespace basis::plugins::transport {

struct Epoll {
  // TODO: rewrite this comment block
  // https://stackoverflow.com/questions/39173429/one-shot-level-triggered-epoll-does-epolloneshot-imply-epollet
  // https://idea.popcount.org/2017-02-20-epoll-is-fundamentally-broken-12/

  // we don't want to be using edge triggered mode per thread
  // it will force us to drain the socket completely
  // if this happens enough times we will run out of available workers

  // instead, level triggered once, but with a work queue
  // one thread pool
  // one main thread, using epoll_wait
  // when epoll_wait is ready add an event to the queue, can call a condition variable once
  // when a thread wakes up due to cv, it pulls an event off the queue
  // when a thread gets EAGAIN on reading, it rearms the fd
  // when a thread finishes a read without EAGAIN, it puts the work back on the queue to signal that there's more to be
  // done when the thread loops, it locks and checks if there's work to be done, no need to wait for cv

  // todo: check for CAP_BLOCK_SUSPEND
  Epoll();

  /*
  Epoll(const Epoll&) = delete;
  Epoll& operator=(const Epoll&) = delete;
  Epoll(Epoll&& other) = default;
  Epoll& operator=(Epoll&& other) = default;
  */
  ~Epoll();

  /**
   * Adds the fd to the watch interface.
   * Forces the fd to be non-blocking.
   * Do _not_ pass duplicated file handles into here. See
   * https://idea.popcount.org/2017-03-20-epoll-is-fundamentally-broken-22/ Do _not_ call in the middle of a callback
   * from epoll (keep your callbacks simple!) Note that it is safe to close() sockets that are registered with epoll,
   * but callbacks may take time to remove themselves.
   */
  using CallbackType = std::function<void(int, std::unique_lock<std::mutex>)>;
  bool AddFd(int fd, CallbackType callback);

  void RemoveFd(int fd);

  bool ReactivateHandle(int fd);

private:
  void MainThread();
  // todo https://idea.popcount.org/2017-03-20-epoll-is-fundamentally-broken-22/
  // https://lwn.net/Articles/520012/
  std::thread epoll_main_thread;
  int epoll_fd = -1;
  std::atomic<bool> stop = false;

  struct CallbackContext {
    CallbackType callback;
    std::mutex mutex;
  };

  std::mutex callbacks_mutex;
  std::unordered_map<int, CallbackContext> callbacks;
};

} // namespace basis::plugins::transport