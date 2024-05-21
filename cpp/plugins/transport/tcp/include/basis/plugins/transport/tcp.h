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
  virtual void SendMessage(std::shared_ptr<core::transport::MessagePacket> message) override;

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
  friend class TestTcpTransport;
  virtual bool Send(const std::byte *data, size_t len) override;

private:
  void StartThread();

  core::networking::TcpSocket socket;

  std::thread send_thread;
  std::condition_variable send_cv;
  std::mutex send_mutex;
  std::vector<std::shared_ptr<const core::transport::MessagePacket>> send_buffer;
  std::atomic<bool> stop_thread = false;
};

class TcpPublisher : public core::transport::TransportPublisher {
public:
  // returns a managed pointer due to mutex member
  static std::expected<std::shared_ptr<TcpPublisher>, core::networking::Socket::Error> Create(uint16_t port = 0);

  // TODO: this should probably just be Update() and also check for dead subscriptions
  size_t CheckForNewSubscriptions();

  uint16_t GetPort();

  virtual std::string GetPublisherInfo() override { return std::to_string(GetPort()); }

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
  TcpTransport(std::shared_ptr<basis::core::transport::ThreadPoolManager> thread_pool_manager)
      : core::transport::Transport(thread_pool_manager) {}

  virtual std::shared_ptr<basis::core::transport::TransportPublisher>
  Advertise(std::string_view topic, [[maybe_unused]] core::transport::MessageTypeInfo type_info) {
    std::shared_ptr<TcpPublisher> publisher = *TcpPublisher::Create();
    {
      std::lock_guard lock(publishers_mutex);
      publishers.emplace(std::string(topic), publisher);
    }
    return std::shared_ptr<basis::core::transport::TransportPublisher>(std::move(publisher));
  };

  virtual std::shared_ptr<basis::core::transport::TransportSubscriber>
  Subscribe(core::transport::OutputQueue* output_queue, std::string_view topic, [[maybe_unused]] core::transport::MessageTypeInfo type_info) {
    // TODO: specify thread pool name
    // TODO: pass in the thread pool every time
    std::shared_ptr<TcpSubscriber> subscriber =
        *TcpSubscriber::Create(topic, &epoll, thread_pool_manager->GetDefaultThreadPool().get(), output_queue);
    {
      std::lock_guard lock(subscribers_mutex);
      subscribers.emplace(std::string(topic), subscriber);
    }
    return std::shared_ptr<basis::core::transport::TransportSubscriber>(std::move(subscriber));
  }
  
  void Update() {
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

// TODO: store list of known publishers?

  /// One epoll instance is shared across the whole TcpTransport - it's an implementation detail of tcp, even if we
  /// could share with other transports
  Epoll epoll;
  /// Worker pools are per-thread group
  // std::unordered_map<std::string, ThreadPool> worker_pools;
};

} // namespace basis::plugins::transport