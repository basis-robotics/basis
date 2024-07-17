#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>

#include "epoll.h"
#include <basis/core/networking/socket.h>
#include <basis/core/transport/subscriber.h>
#include <basis/core/transport/transport.h>

#include "tcp_connection.h"

namespace basis::plugins::transport {

struct AddressPortHash {
  size_t operator()(std::pair<std::string, uint16_t> p) const noexcept {
    return std::hash<std::string>{}(p.first) ^ std::hash<uint16_t>{}(p.second);
  }
};

/**
 * Used to receive serialized data over TCP.
 *
 * @todo these could be pooled. If multiple subscribers to the same topic are created, we should only have to recieve
 * once. It's a bit of an early optimization, though.
 *
 * @todo this class is fairly useless, now - we can juts utilize TcpConnection's Socket constructor
 */
class TcpReceiver : public TcpConnection {
public:
  TcpReceiver(std::string_view address, uint16_t port) : address(address), port(port) {}

  bool Connect() {
    auto maybe_socket = core::networking::TcpSocket::Connect(address, port);
    if (maybe_socket) {
      socket = std::move(maybe_socket.value());
      return true;
    }
    return false;
  }

  const core::networking::Socket &GetSocket() const { return socket; }

private:
  std::string address;
  uint16_t port;
};

class TcpSubscriber : public core::transport::TransportSubscriber {
public:
  // todo: error condition
  static nonstd::expected<std::shared_ptr<TcpSubscriber>, core::networking::Socket::Error>
  Create(std::string_view topic_name, core::transport::TypeErasedSubscriberCallback callback, Epoll *epoll,
         core::threading::ThreadPool *worker_pool, std::vector<std::pair<std::string_view, uint16_t>> addresses = {});

  virtual bool Connect(std::string_view host, std::string_view endpoint, __uint128_t publisher_id) override;

  // todo: error handling
  bool ConnectToPort(std::string_view address, uint16_t port);

  virtual size_t GetPublisherCount() override;

protected:
  TcpSubscriber(std::string_view topic_name, core::transport::TypeErasedSubscriberCallback callback, Epoll *epoll,
                core::threading::ThreadPool *worker_pool);

  std::string topic_name;
  core::transport::TypeErasedSubscriberCallback callback;

  Epoll *epoll;
  core::threading::ThreadPool *worker_pool;
  std::unordered_map<std::pair<std::string, uint16_t>, TcpReceiver, AddressPortHash> receivers = {};
};

} // namespace basis::plugins::transport