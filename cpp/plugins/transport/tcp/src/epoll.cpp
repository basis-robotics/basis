#include <basis/plugins/transport/epoll.h>

namespace basis::plugins::transport {

  Epoll::Epoll() {
    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    assert(epoll_fd != -1);

    epoll_main_thread = std::thread(&Epoll::MainThread, this);
  }

  Epoll::~Epoll() {
    SPDLOG_DEBUG("~Epoll");
    stop = true;
    if (epoll_main_thread.joinable()) {
      epoll_main_thread.join();
    }
    close(epoll_fd);
  }

  void Epoll::MainThread() {
    // https://linux.die.net/man/4/epoll
    constexpr int MAX_EVENTS = 128;
    epoll_event events[128];
    while (!stop) {
      // Wait for events, indefinitely, checking once per second for shutdown
      // todo: we could send a signal or add an additional socket for epoll to listen to
      // to get faster shutdown
      SPDLOG_DEBUG("epoll_wait");
      int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
      if (nfds < 0) {
        SPDLOG_DEBUG("epoll dieing");
        return;
      }
      SPDLOG_DEBUG("epoll_wait nfds {}", nfds);
      for (int n = 0; n < nfds; ++n) {
        int fd = events[n].data.fd;
        spdlog::info("Socket {} ready.", events[n].data.fd);
        callbacks.at(fd)(fd);
      }
    }
  }

  /**
   * @todo would it be useful to allow blocking sockets here?
   * @todo should we enforce a Socket& instead?
   * @todo need to ensure that we remove the handle from epoll before we close it. This means a two way reference
   * @todo a careful reading of the epoll spec implies that one should read from the socket once after adding here
   * @todo error handling
   * @todo return a handle that can reactivate itself
   */
  bool Epoll::AddFd(int fd, std::function<void(int)> callback) {
    assert(callbacks.count(fd) == 0);
    SPDLOG_DEBUG("AddFd {}", fd);
    int flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    epoll_event event;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT; // EPOLLEXCLUSIVE shouldn't be needed for single threaded epoll
    event.data.fd = fd;

    // This is safe across threads
    // https://bugzilla.kernel.org/show_bug.cgi?id=43072
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event)) {
      SPDLOG_DEBUG("Failed to add file descriptor to epoll");
      return false;
    }
    callbacks[fd] = callback;
    return true;
  }

  bool Epoll::ReactivateHandle(int fd) {
    epoll_event event;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    event.data.fd = fd;

    // This is safe across threads
    // https://bugzilla.kernel.org/show_bug.cgi?id=43072
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event)) {
      SPDLOG_DEBUG("Failed to wake up fd with epoll");
      return false;
    }
    return true;
  }
 
}