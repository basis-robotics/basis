#include <basis/plugins/transport/tcp_subscriber.h>
#include <basis/plugins/transport/tcp_transport_name.h>
#include <errno.h>
#include <string.h>
#include <charconv>
#include <functional>
#include <mutex>
#include <system_error>
#include <type_traits>

#include "basis/core/networking/socket.h"
#include "basis/core/threading/thread_pool.h"
#include "basis/core/transport/subscriber.h"
#include "basis/core/transport/transport.h"
#include "basis/plugins/transport/epoll.h"
#include "spdlog/fmt/bundled/core.h"
#include "spdlog/spdlog.h"

namespace basis::plugins::transport {

nonstd::expected<std::shared_ptr<TcpSubscriber>, core::networking::Socket::Error>
TcpSubscriber::Create(std::string_view topic_name, core::transport::TypeErasedSubscriberCallback callback, Epoll *epoll,
                      core::threading::ThreadPool *worker_pool, core::transport::OutputQueue *output_queue,
                      std::vector<std::pair<std::string_view, uint16_t>> addresses) {
  auto subscriber = std::shared_ptr<TcpSubscriber>(
      new TcpSubscriber(topic_name, std::move(callback), epoll, worker_pool, output_queue));
  for (auto &[address, port] : addresses) {
    subscriber->ConnectToPort(address, port);
  }
  return subscriber;
}

TcpSubscriber::TcpSubscriber(std::string_view topic_name, core::transport::TypeErasedSubscriberCallback callback,
                             Epoll *epoll, core::threading::ThreadPool *worker_pool,
                             core::transport::OutputQueue *output_queue)
    : core::transport::TransportSubscriber(TCP_TRANSPORT_NAME), topic_name(topic_name), callback(callback),
      epoll(epoll), worker_pool(worker_pool), output_queue(output_queue) {}

bool TcpSubscriber::Connect(std::string_view host, std::string_view endpoint,
                            [[maybe_unused]] __uint128_t publisher_id) {
  uint16_t port;
  auto result = std::from_chars(endpoint.data(), endpoint.data() + endpoint.size(), port);
  if (result.ec != std::errc()) {
    spdlog::error("TcpSubscriber::Connect: '{}' is not a valid port", endpoint);
    return false;
  }

  /// @todo BASIS-13: this should take in a publisher ID and send it as part of the intial connection, protecting
  /// against subscribing to a port that's stale
  return ConnectToPort(host, port);
}

bool TcpSubscriber::ConnectToPort(std::string_view address, uint16_t port) {
  std::pair<std::string, uint16_t> key(address, port);
  {
    if (receivers.count(key) != 0) {
      spdlog::warn("Already have address {}:{}", address, port);
      return true;
    }

    auto receiver = TcpReceiver(address, port);
    if (!receiver.Connect()) {
      spdlog::error("Unable to connect to {}:{}", address, port);

      return false;
    }

    // TODO now hook up to epoll!
    receivers.emplace(key, std::move(receiver));
  }

  auto on_epoll_callback = [this](int fd, std::unique_lock<std::mutex> lock, TcpReceiver *receiver_ptr,
                                  std::shared_ptr<core::transport::IncompleteMessagePacket> incomplete) {
    spdlog::debug("Queuing work for socket {}", fd);

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

  return true;
}
size_t TcpSubscriber::GetPublisherCount() { return receivers.size(); }
} // namespace basis::plugins::transport