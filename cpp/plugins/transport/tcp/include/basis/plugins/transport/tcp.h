#pragma once

#include <condition_variable>
#include <thread>
#include <vector>

#include "epoll.h"
#include <basis/core/networking/socket.h>
#include <basis/core/transport/publisher.h>
#include <basis/core/transport/subscriber.h>
#include <basis/core/transport/transport.h>
namespace basis::plugins::transport {

/**
 * Used to send serialized data over TCP.
 *
 * Basic building block for sending data over the network.
 *
 * Currently spawns thread per publisher. This should be performant but will waste memory in stack allocations.
 *
 * @todo Move to using a worker/thread pool - should be relatively easy.
 */
class TcpSender : public core::transport::TransportSender {
public:
  /**
   * Construct a sender, given an already created+valid socket.
   */
  TcpSender(core::networking::TcpSocket socket) : socket(std::move(socket)) { StartThread(); }

  /**
   * Destruct.
   */
  ~TcpSender() {
    // Do _not_ manually call Close() here if running asynchronously.
    // Doing so could lead to race conditions. (TODO: why?)
    // socket.Close();
    Stop(true);
  }

  bool IsConnected() {
    // TODO: this does not handle failure cases - needs to query socket internally for validity
    return socket.IsValid();
  }

  // TODO: do we want to be able to send high priority packets?
  virtual void SendMessage(std::shared_ptr<core::transport::RawMessage> message) override;

  void Stop(bool wait = false) {
    stop_thread = true;

    send_cv.notify_one();
    if (wait) {
      if (send_thread.joinable()) {
        send_thread.join();
      }
    }
  }

protected:
  friend class TestTcpTransport;
  virtual bool Send(const std::byte *data, size_t len) override;

private:
  void StartThread();

  core::networking::TcpSocket socket;

  std::thread send_thread;
  std::condition_variable send_cv;
  std::mutex send_mutex;
  std::vector<std::shared_ptr<const core::transport::RawMessage>> send_buffer;
  std::atomic<bool> stop_thread = false;
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
  std::unique_ptr<const core::transport::RawMessage> ReceiveMessage(int timeout_s);

  // todo: standardized class for handling these
  enum class ReceiveStatus { DOWNLOADING, DONE, ERROR, DISCONNECTED };
  ReceiveStatus ReceiveMessage(core::transport::IncompleteRawMessage &message);

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

class TcpPublisher : public core::transport::TransportPublisher {
public:
  static std::expected<TcpPublisher, core::networking::Socket::Error> Create(uint16_t port = 0);

  size_t CheckForNewSubscriptions();

  uint16_t GetPort();

protected:
  TcpPublisher(core::networking::TcpListenSocket listen_socket);

  core::networking::TcpListenSocket listen_socket;
  std::vector<std::unique_ptr<TcpSender>> senders;
};

class TcpSubscriber : public core::transport::TransportSubscriber {
public:
  static std::expected<TcpPublisher, core::networking::Socket::Error> Create(uint16_t port = 0);

protected:
  TcpSubscriber(Epoll *epoll, core::threading::ThreadPool *work);
};

class TcpTransport : public core::transport::Transport {
public:
  TcpTransport(std::shared_ptr<basis::core::transport::ThreadPoolManager> thread_pool_manager)
      : core::transport::Transport(thread_pool_manager) {}

  virtual std::shared_ptr<basis::core::transport::TransportPublisher> Advertise(std::string_view topic, [[maybe_unused]] core::transport::MessageTypeInfo type_info) {
    std::shared_ptr<TcpPublisher> publisher = std::make_shared<TcpPublisher>(*TcpPublisher::Create());
    publishers.emplace(std::string(topic), publisher);
    return std::shared_ptr<basis::core::transport::TransportPublisher>(std::move(publisher));
  };

  // todo subscriptions
  // virtual std::unique_ptr<TransportSubscriber> Subscribe(std::string_view topic) = 0;
private:
  std::unordered_multimap<std::string, std::weak_ptr<TcpPublisher>> publishers;
  /// One epoll instance is shared across the whole TcpTransport - it's an implementation detail of tcp, even if we
  /// could share with other transports
  Epoll epoll;
  /// Worker pools are per-thread group
  // std::unordered_map<std::string, ThreadPool> worker_pools;
};

} // namespace basis::plugins::transport