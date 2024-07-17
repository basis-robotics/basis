#pragma once

#include <condition_variable>
#include <thread>
#include <vector>

#include "epoll.h"
#include <basis/core/networking/socket.h>
#include <basis/core/transport/publisher.h>
#include <basis/core/transport/subscriber.h>
#include <basis/core/transport/transport.h>

#include "tcp_subscriber.h"
#include "tcp_transport_name.h"

class TestTcpTransport;

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
class TcpSender : public TcpConnection {
public:
  /**
   * Construct a sender, given an already created+valid socket.
   */
  TcpSender(core::networking::TcpSocket socket) :  TcpConnection(std::move(socket)) { 
    StartThread(); }

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
  void SendMessage(std::shared_ptr<core::transport::MessagePacket> message);

  void Stop(bool wait = false) {
    {
      std::lock_guard lock(send_mutex);
      stop_thread = true;
    }
    send_cv.notify_one();
    if (wait) {
      if (send_thread.joinable()) {
        send_thread.join();
      }
    }
  }

protected:
  friend class ::TestTcpTransport;

private:
  void StartThread();

  std::thread send_thread;
  std::condition_variable send_cv;
  std::mutex send_mutex;
  std::vector<std::shared_ptr<const core::transport::MessagePacket>> send_buffer;
  std::atomic<bool> stop_thread = false;
};

class TcpPublisher : public core::transport::TransportPublisher {
public:
  // returns a managed pointer due to mutex member
  static nonstd::expected<std::shared_ptr<TcpPublisher>, core::networking::Socket::Error> Create(uint16_t port = 0);

  // TODO: this should probably just be Update() and also check for dead subscriptions
  size_t CheckForNewSubscriptions();

  uint16_t GetPort();

  virtual std::string GetTransportName() override { return TCP_TRANSPORT_NAME; }

  virtual std::string GetConnectionInformation() override { return std::to_string(GetPort()); }

  virtual void SendMessage(std::shared_ptr<core::transport::MessagePacket> message) override;

  virtual size_t GetSubscriberCount() override {
    std::lock_guard lock(senders_mutex);
    return senders.size();
  }

protected:
  TcpPublisher(core::networking::TcpListenSocket listen_socket);

  core::networking::TcpListenSocket listen_socket;
  std::mutex senders_mutex;
  std::vector<std::unique_ptr<TcpSender>> senders;
};

// todo: need to catch out of order subscribe/publishes
class TcpTransport : public core::transport::Transport {
public:
  TcpTransport() {}

  virtual std::shared_ptr<basis::core::transport::TransportPublisher>
  Advertise(std::string_view topic, [[maybe_unused]] core::serialization::MessageTypeInfo type_info) override {
    return Advertise(topic, type_info, 0);
  }

  std::shared_ptr<TcpPublisher>
  Advertise(std::string_view topic, [[maybe_unused]] core::serialization::MessageTypeInfo type_info, uint16_t port) {
    std::shared_ptr<TcpPublisher> publisher = *TcpPublisher::Create(port);
    {
      std::lock_guard lock(publishers_mutex);
      publishers.emplace(std::string(topic), publisher);
    }
    return publisher;
  }

  virtual std::shared_ptr<basis::core::transport::TransportSubscriber>
  Subscribe(std::string_view topic, core::transport::TypeErasedSubscriberCallback callback,
            basis::core::threading::ThreadPool* work_thread_pool,
            [[maybe_unused]] core::serialization::MessageTypeInfo type_info) override {
    // TODO: pass in the thread pool every time
    // TODO: error handling
    std::shared_ptr<TcpSubscriber> subscriber = *TcpSubscriber::Create(
        topic, std::move(callback), &epoll, work_thread_pool);
    {
      std::lock_guard lock(subscribers_mutex);
      subscribers.emplace(std::string(topic), subscriber);
    }
    return std::shared_ptr<basis::core::transport::TransportSubscriber>(std::move(subscriber));
  }

  virtual void Update() override {
    decltype(publishers)::iterator it;
    {
      std::lock_guard lock(publishers_mutex);
      it = publishers.begin();
    }
    // For each publisher
    while (it != publishers.end()) {
      {
        auto pub = it->second.lock();
        if (!pub) {
          // If this publisher has been destructed, drop it
          std::lock_guard lock(publishers_mutex);
          it = publishers.erase(it);
        } else {
          // Otherwise, check if it has no subscribers
          pub->CheckForNewSubscriptions();

          // Iterate, move on
          std::lock_guard lock(publishers_mutex);
          it++;
        }
      }
    }
  }

private:
  std::mutex publishers_mutex;

  std::unordered_multimap<std::string, std::weak_ptr<TcpPublisher>> publishers;

  std::mutex subscribers_mutex;
  std::unordered_multimap<std::string, std::weak_ptr<TcpSubscriber>> subscribers;

  /// One epoll instance is shared across the whole TcpTransport - it's an implementation detail of tcp, even if we
  /// could share with other transports
  Epoll epoll;
  /// Worker pools are per-thread group
  // std::unordered_map<std::string, ThreadPool> worker_pools;
};

} // namespace basis::plugins::transport