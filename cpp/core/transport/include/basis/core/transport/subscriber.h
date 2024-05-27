#pragma once

#include "inproc.h"
#include "message_event.h"
#include "message_packet.h"
#include "message_type_info.h"
#include "publisher_info.h"

#include <basis/core/time.h>
#include <functional>
#include <memory>

class TestTcpTransport;

namespace basis {
namespace core {
namespace transport {

template <typename T_MSG> using SubscriberCallback = std::function<void(std::shared_ptr<const T_MSG>)>;
// TODO: this can almost certainly be a unique ptr
using TypeErasedSubscriberCallback = std::function<void(std::unique_ptr<MessagePacket>)>;

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

/**
 * @brief SubscriberBase - used to type erase Subscriber
 *
 */
class SubscriberBase {
protected:
  SubscriberBase(
    std::string_view topic, MessageTypeInfo type_info, bool has_inproc,
             std::vector<std::shared_ptr<TransportSubscriber>> transport_subscribers) :
               topic(topic), type_info(std::move(type_info)), has_inproc(has_inproc), transport_subscribers(std::move(transport_subscribers)) {

             }
public:

  virtual ~SubscriberBase() = default;

  /**
   * Notify this subscriber of one or more publishers.
   * 
   * The subscriber will pass the relevant information down into the correct transports.
   */
  void HandlePublisherInfo(const std::vector<PublisherInfo>& info);

  size_t GetPublisherCount();
protected:
  friend class ::TestTcpTransport;
  const std::string topic;
  const MessageTypeInfo type_info;

  const bool has_inproc;

  // TODO: these are shared_ptrs - it could be a single unique_ptr if we were sure we never want to pool these
  std::vector<std::shared_ptr<TransportSubscriber>> transport_subscribers;

  /**
   * Map associating a publisher ID to a transport that is assigned to handle it.
   * nullptr is valid and is a sentinal value for the inproc transport.
   */
  std::unordered_map<__uint128_t, TransportSubscriber*> publisher_id_to_transport_sub;
};

template <typename T_MSG> class Subscriber : public SubscriberBase {
public:
  Subscriber(std::string_view topic, MessageTypeInfo type_info,
             std::vector<std::shared_ptr<TransportSubscriber>> transport_subscribers,
             std::shared_ptr<InprocSubscriber<T_MSG>> inproc)
      : SubscriberBase(topic, std::move(type_info), inproc != nullptr, std::move(transport_subscribers)),
        inproc(std::move(inproc)) {}


  std::shared_ptr<InprocSubscriber<T_MSG>> inproc;
};

} // namespace transport
} // namespace core
} // namespace basis