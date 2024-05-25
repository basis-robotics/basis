#include <basis/plugins/transport/tcp_subscriber.h>

namespace basis::plugins::transport {

nonstd::expected<std::shared_ptr<TcpSubscriber>, core::networking::Socket::Error>
TcpSubscriber::Create(std::string_view topic_name, core::transport::TypeErasedSubscriberCallback callback, Epoll *epoll,
                      core::threading::ThreadPool *worker_pool, core::transport::OutputQueue *output_queue,
                      std::vector<std::pair<std::string_view, uint16_t>> addresses) {
  auto subscriber = std::shared_ptr<TcpSubscriber>(
      new TcpSubscriber(topic_name, std::move(callback), epoll, worker_pool, output_queue));
  for (auto &[address, port] : addresses) {
    subscriber->Connect(address, port);
  }
  return subscriber;
}

TcpSubscriber::TcpSubscriber(std::string_view topic_name, core::transport::TypeErasedSubscriberCallback callback,
                             Epoll *epoll, core::threading::ThreadPool *worker_pool,
                             core::transport::OutputQueue *output_queue)
    : topic_name(topic_name), callback(callback), epoll(epoll), worker_pool(worker_pool), output_queue(output_queue) {}

void TcpSubscriber::Connect(std::string_view address, uint16_t port) {
  std::pair<std::string, uint16_t> key(address, port);
  {
    if (receivers.count(key) != 0) {
      spdlog::warn("Already have address {}:{}", address, port);
      return;
    }

    auto receiver = TcpReceiver(address, port);
    if (!receiver.Connect()) {
      spdlog::error("Unable to connect to {}:{}", address, port);

      return;
    }

    // TODO now hook up to epoll!
    receivers.emplace(key, std::move(receiver));
  }

  auto on_epoll_callback = [this](int fd, std::unique_lock<std::mutex> lock, TcpReceiver *receiver_ptr,
                                  std::shared_ptr<core::transport::IncompleteMessagePacket> incomplete) {
    spdlog::info("Queuing work for {}", fd);

    worker_pool->enqueue([this, fd, incomplete = std::move(incomplete), receiver_ptr, lock = std::move(lock)] {
      // It's an error to actually call this with multiple threads.
      // TODO: add debug only checks for this
      switch (receiver_ptr->ReceiveMessage(*incomplete)) {

      case TcpReceiver::ReceiveStatus::DONE: {
        if (output_queue) {
          output_queue->Emplace({topic_name, incomplete->GetCompletedMessage(), callback});
        } else {
          // TODO: this still isn't quite correct - we need to define three policies
          // 1. Put into work queue, rely on queue to be processed
          // 2. Put into local queue, rely on someone servicing the subscriber
          // 3. Call immediately
          this->callback(incomplete->GetCompletedMessage());
        }
      }
      case TcpReceiver::ReceiveStatus::DOWNLOADING: {
        // No work to be done
        break;
      }
      case TcpReceiver::ReceiveStatus::ERROR: {
        // TODO
        spdlog::error("{}, {}: bytes {} - got error {} {}", fd, (void *)incomplete.get(),
                      incomplete->GetCurrentProgress(), errno, strerror(errno));
      }
      case TcpReceiver::ReceiveStatus::DISCONNECTED: {

        // TODO
        spdlog::error("Disconnecting from topic {}", topic_name);
        return;
      }
      }
      epoll->ReactivateHandle(fd);
    });
  };

  TcpReceiver *receiver_ptr = &receivers.at(key);
  epoll->AddFd(receiver_ptr->GetSocket().GetFd(),
               std::bind(on_epoll_callback, std::placeholders::_1, std::placeholders::_2, receiver_ptr,
                         std::make_shared<core::transport::IncompleteMessagePacket>()));
}

} // namespace basis::plugins::transport