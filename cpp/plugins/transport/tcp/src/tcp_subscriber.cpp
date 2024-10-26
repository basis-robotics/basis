#include <basis/plugins/transport/logger.h>
#include <basis/plugins/transport/tcp_subscriber.h>
#include <basis/plugins/transport/tcp_transport_name.h>

#include <charconv>

namespace basis::plugins::transport {

using namespace tcp;

TcpSubscriber::~TcpSubscriber() {
  for (const auto &[_, receiver] : receivers) {
    epoll->RemoveFd(receiver.GetSocket().GetFd());
  }
}

nonstd::expected<std::shared_ptr<TcpSubscriber>, core::networking::Socket::Error>
TcpSubscriber::Create(std::string_view topic_name, core::transport::TypeErasedSubscriberCallback callback, Epoll *epoll,
                      core::threading::ThreadPool *worker_pool,
                      std::vector<std::pair<std::string_view, uint16_t>> addresses) {
  auto subscriber =
      std::shared_ptr<TcpSubscriber>(new TcpSubscriber(topic_name, std::move(callback), epoll, worker_pool));
  for (auto &[address, port] : addresses) {
    subscriber->ConnectToPort(address, port);
  }
  return subscriber;
}

TcpSubscriber::TcpSubscriber(std::string_view topic_name, core::transport::TypeErasedSubscriberCallback callback,
                             Epoll *epoll, core::threading::ThreadPool *worker_pool)
    : core::transport::TransportSubscriber(TCP_TRANSPORT_NAME), topic_name(topic_name), callback(callback),
      epoll(epoll), worker_pool(worker_pool) {}

bool TcpSubscriber::Connect(std::string_view host, std::string_view endpoint,
                            [[maybe_unused]] __uint128_t publisher_id) {
  uint16_t port;
  auto result = std::from_chars(endpoint.data(), endpoint.data() + endpoint.size(), port);
  if (result.ec != std::errc()) {
    BASIS_LOG_ERROR("TcpSubscriber::Connect: '{}' is not a valid port", endpoint);
    return false;
  }

  /// @todo BASIS-13: this should take in a publisher ID and send it as part of the initial connection, protecting
  /// against subscribing to a port that's stale
  return ConnectToPort(host, port);
}

bool TcpSubscriber::ConnectToPort(std::string_view address, uint16_t port) {
  std::pair<std::string, uint16_t> key(address, port);
  {
    if (receivers.count(key) != 0) {
      BASIS_LOG_WARN("Already have address {}:{}", address, port);
      return true;
    }

    auto receiver = TcpReceiver(address, port);
    if (!receiver.Connect()) {
      BASIS_LOG_ERROR("Unable to connect to {}:{}", address, port);

      return false;
    }

    // TODO now hook up to epoll!
    receivers.emplace(key, std::move(receiver));
  }

  auto on_epoll_callback = [this, key](int fd, std::unique_lock<std::mutex> lock, TcpReceiver *receiver_ptr,
                                  std::shared_ptr<core::transport::IncompleteMessagePacket> incomplete) {
    BASIS_LOG_DEBUG("Queuing work for socket {}", fd);

    worker_pool->enqueue([this, fd, incomplete = std::move(incomplete), receiver_ptr, lock = std::move(lock), key] {
      // It's an error to actually call this with multiple threads.
      // TODO: add debug only checks for this
      switch (receiver_ptr->ReceiveMessage(*incomplete)) {

      case TcpReceiver::ReceiveStatus::DONE: {
        this->callback(incomplete->GetCompletedMessage());
      }
      case TcpReceiver::ReceiveStatus::DOWNLOADING: {
        // No work to be done
        break;
      }
      case TcpReceiver::ReceiveStatus::ERROR: {
        // TODO
        BASIS_LOG_ERROR("{}, {}: bytes {} - got error {} {}", fd, (void *)incomplete.get(),
                        incomplete->GetCurrentProgress(), errno, strerror(errno));
      }
      case TcpReceiver::ReceiveStatus::DISCONNECTED: {
        // TODO: this needs to be updated when we gracefully handle disconnection
        BASIS_LOG_ERROR("Disconnecting from topic {}", topic_name);
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

  return true;
}
size_t TcpSubscriber::GetPublisherCount() { return receivers.size(); }
} // namespace basis::plugins::transport