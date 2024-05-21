#include <basis/plugins/transport/tcp_subscriber.h>

namespace basis::plugins::transport {
TcpSubscriber::TcpSubscriber(Epoll *epoll, core::threading::ThreadPool *worker_pool)
    : epoll(epoll), worker_pool(worker_pool) {}

void TcpSubscriber::Connect(std::string_view address, uint16_t port) {
  std::pair<std::string, uint16_t> key(address, port);
  if (receivers.count(key) != 0) {
    return;
  }

  auto receiver = TcpReceiver(address, port);
  if (!receiver.Connect()) {
    return;
  }

  //TODO now hook up to epoll!
  //but first split this file up please
  receivers.emplace(std::move(key), std::move(receiver));
#if 0
  auto callback = [&thread_pool, this, &output_queue](
                      int fd, std::unique_lock<std::mutex> lock, std::string channel_name, TcpReceiver *receiver,
                      std::shared_ptr<core::transport::IncompleteMessagePacket> incomplete) {
    spdlog::info("Queuing work for {}", fd);

    /**
     * This is called by epoll when new data is available on a socket. We immediately do nothing with it, and instead
     * push the work off to the thread pool. This should be a very fast operation.
     */
    thread_pool.enqueue([fd, receiver, channel_name, this, &output_queue, incomplete, lock = std::move(lock)] {
      // It's an error to actually call this with multiple threads.
      // TODO: add debug only checks for this
      spdlog::debug("Running thread pool callback on {}", fd);
      switch (receiver->ReceiveMessage(*incomplete)) {

      case TcpReceiver::ReceiveStatus::DONE: {
        spdlog::debug("TcpReceiver Got full message");
        auto msg = incomplete->GetCompletedMessage();

        std::pair<std::string, std::shared_ptr<core::transport::MessagePacket>> out(channel_name, std::move(msg));
        output_queue.Emplace(std::move(out));

        // TODO: peek
        break;
      }
      case TcpReceiver::ReceiveStatus::DOWNLOADING: {
        break;
      }
      case TcpReceiver::ReceiveStatus::ERROR: {
        spdlog::error("{}, {}: bytes {} - got error {} {}", fd, (void *)incomplete.get(),
                      incomplete->GetCurrentProgress(), errno, strerror(errno));
      }
      case TcpReceiver::ReceiveStatus::DISCONNECTED: {
        spdlog::error("Disconnecting from channel {}", channel_name);
        return;
      }
      }
      poller->ReactivateHandle(fd);
      spdlog::info("Rearmed");
    });
  };
  #endif
}

std::expected<std::shared_ptr<TcpSubscriber>, core::networking::Socket::Error>
TcpSubscriber::Create(Epoll *epoll, core::threading::ThreadPool *worker_pool,
                      std::vector<std::pair<std::string_view, uint16_t>> addresses) {
  auto subscriber = std::shared_ptr<TcpSubscriber>(new TcpSubscriber(epoll, worker_pool));
  for (auto &[address, port] : addresses) {
    subscriber->Connect(address, port);
  }
  return subscriber;
}

}