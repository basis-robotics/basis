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
public:
  virtual ~TransportSubscriber() = default;
};

/**
 * @brief SubscriberBase - used to type erase Subscriber
 *
 */
class SubscriberBase {
protected:
  SubscriberBase(
    std::string_view topic, MessageTypeInfo type_info,
             std::vector<std::shared_ptr<TransportSubscriber>> transport_subscribers) :
               topic(topic), type_info(std::move(type_info)), transport_subscribers(std::move(transport_subscribers)) {

             }
public:

  /**
   * Notify this subscriber of one or more publishers.
   * 
   * The subscriber will pass the relevant information down into the correct transports.
   */
  void HandlePublisherInfo(const std::vector<PublisherInfo>& info);

  virtual ~SubscriberBase() = default;

protected:
  friend class ::TestTcpTransport;
  std::string topic;
  MessageTypeInfo type_info;
  std::vector<std::shared_ptr<TransportSubscriber>> transport_subscribers;

};

template <typename T_MSG> class Subscriber : public SubscriberBase {
public:
  Subscriber(std::string_view topic, MessageTypeInfo type_info,
             std::vector<std::shared_ptr<TransportSubscriber>> transport_subscribers,
             std::shared_ptr<InprocSubscriber<T_MSG>> inproc)
      : SubscriberBase(topic, std::move(type_info), std::move(transport_subscribers)),
        inproc(std::move(inproc)) {}


  // TODO: these are shared_ptrs - it could be a single unique_ptr if we were sure we never want to pool these
  std::shared_ptr<InprocSubscriber<T_MSG>> inproc;
};

} // namespace transport
} // namespace core
} // namespace basis