#pragma once

#include "inproc.h"
#include "message_event.h"
#include "message_packet.h"
#include "publisher_info.h"
#include <basis/core/serialization/message_type_info.h>

#include <basis/core/time.h>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_map>

class TestTcpTransport;

namespace basis {
namespace core {
namespace transport {

template <typename T_MSG> using SubscriberCallback = std::function<void(std::shared_ptr<const T_MSG>)>;
using TypeErasedSubscriberCallback = std::function<void(std::shared_ptr<MessagePacket>)>;

class TransportSubscriber {
protected:
  TransportSubscriber(std::string_view transport_name) : transport_name(transport_name) {}

public:
  std::string_view GetTransportName() const { return transport_name; }

  virtual bool Connect(std::string_view host, std::string_view endpoint, __uint128_t publisher_id) = 0;

  virtual size_t GetPublisherCount() = 0;

  virtual ~TransportSubscriber() = default;
  const std::string transport_name;
};

struct Hash128 {
    size_t operator()(__uint128_t var) const {
        return std::hash<uint64_t>{}((uint64_t)var ^ (uint64_t)(var >> 64));
    }
};

/**
 * @brief SubscriberBase - used to type erase Subscriber
 *
 */
class SubscriberBase {
public:
  SubscriberBase(std::string_view topic, serialization::MessageTypeInfo type_info,
                 std::vector<std::shared_ptr<TransportSubscriber>> transport_subscribers, bool has_inproc)
      : topic(topic), type_info(std::move(type_info)), has_inproc(has_inproc),
        transport_subscribers(std::move(transport_subscribers)) {}

  virtual ~SubscriberBase() = default;

  /**
   * Notify this subscriber of one or more publishers.
   *
   * The subscriber will pass the relevant information down into the correct transports.
   */
  void HandlePublisherInfo(const std::vector<PublisherInfo> &info);

  size_t GetPublisherCount();

protected:
  friend class ::TestTcpTransport;
  const std::string topic;
  const serialization::MessageTypeInfo type_info;

  const bool has_inproc;

  // TODO: these are shared_ptrs - it could be a single unique_ptr if we were sure we never want to pool these
  std::vector<std::shared_ptr<TransportSubscriber>> transport_subscribers;

  /**
   * Map associating a publisher ID to a transport that is assigned to handle it.
   * nullptr is valid and is a sentinal value for the inproc transport.
   */
  std::unordered_map<__uint128_t, TransportSubscriber *, Hash128> publisher_id_to_transport_sub;
};

template <typename T_MSG> class Subscriber : public SubscriberBase {
public:
  using MessageType = T_MSG;
  Subscriber(std::string_view topic, serialization::MessageTypeInfo type_info,
             std::vector<std::shared_ptr<TransportSubscriber>> transport_subscribers,
             std::shared_ptr<InprocSubscriber<T_MSG>> inproc)
      : SubscriberBase(topic, std::move(type_info), std::move(transport_subscribers), inproc != nullptr),
        inproc(std::move(inproc)) {}

protected:
  std::shared_ptr<InprocSubscriber<T_MSG>> inproc;
};

class RateSubscriber {
public:
  RateSubscriber(const Duration &tick_length, std::function<void(MonotonicTime)> callback)
      : tick_length(tick_length), callback(std::move(callback)) {
    // auto start?
    Start();
  }

  RateSubscriber() = default;

  ~RateSubscriber() { Stop(); }

  void Start() { thread = std::thread(&RateSubscriber::ThreadFunction, this); }

  void Stop() {
    stop = true;
    if (thread.joinable()) {
      thread.join();
    }
  }

protected:
  void ThreadFunction() {
    const uint64_t run_token = MonotonicTime::GetRunToken();
    MonotonicTime next = MonotonicTime::Now();
    while (!stop) {
      next += tick_length;
      next.SleepUntil(run_token);
      // Really ugly kludge to not run callbacks when we've detected a time jump
      if(run_token != MonotonicTime::GetRunToken()) {
        break;
      }
      // Don't use next here - it will be affected by the scheduler
      callback(MonotonicTime::Now());
      
    }
  }

  Duration tick_length;
  std::atomic<bool> stop;

  std::thread thread;

  std::function<void(MonotonicTime)> callback;
};

} // namespace transport
} // namespace core
} // namespace basis