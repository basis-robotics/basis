#pragma once

#include <cstdint>
#include <unordered_map>
#include <string_view>


#include <basis/core/networking/socket.h>
#include <basis/core/transport/subscriber.h>
#include <basis/core/transport/transport.h>
#include "epoll.h"

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
 */
class TcpReceiver : public core::transport::TransportReceiver {
public:
  TcpReceiver(std::string_view address, uint16_t port) : address(address), port(port) {}

  virtual bool Connect() {
    auto maybe_socket = core::networking::TcpSocket::Connect(address, port);
    if (maybe_socket) {
      socket = std::move(maybe_socket.value());
      return true;
    }
    return false;
  }

  bool IsConnected() const { return socket.IsValid(); }

  /**
   *
   *
   * returns unique as it's expected a transport will handle this.
   * @todo why do we need to return unique? We have a unique ptr wrapping a unique ptr - unneccessary.
   */
  std::unique_ptr<const core::transport::MessagePacket> ReceiveMessage(int timeout_s);

  // todo: standardized class for handling these
  enum class ReceiveStatus { DOWNLOADING, DONE, ERROR, DISCONNECTED };
  ReceiveStatus ReceiveMessage(core::transport::IncompleteMessagePacket &message);

  /**
   * @todo error handling
   */
  virtual bool Receive(std::byte *buffer, size_t buffer_len, int timeout_s = -1) override;

  const core::networking::Socket &GetSocket() const { return socket; }

private:
  core::networking::TcpSocket socket;

  std::string address;
  uint16_t port;
};


class TcpSubscriber : public core::transport::TransportSubscriber {
public:
  // todo: error condition
  static std::expected<std::shared_ptr<TcpSubscriber>, core::networking::Socket::Error>
  Create(Epoll *epoll, core::threading::ThreadPool *worker_pool,
         std::vector<std::pair<std::string_view, uint16_t>> addresses = {});

  // todo: error handling
  void Connect(std::string_view address, uint16_t port);

protected:
  TcpSubscriber(Epoll *epoll, core::threading::ThreadPool *worker_pool);
  Epoll *epoll;
  core::threading::ThreadPool *worker_pool;
  std::unordered_map<std::pair<std::string, uint16_t>, TcpReceiver, AddressPortHash> receivers = {};
};

}