#pragma once

#include <functional>
#include <memory>
#include <basis/core/time.h>
#include "message_event.h"
#include "message_type_info.h"
#include "inproc.h"
#include "message_packet.h"


namespace basis {
namespace core {
namespace transport {

template<typename T_MSG>
using SubscriberCallback = std::function<void(std::shared_ptr<const T_MSG>)>;
// TODO: this can almost certainly be a unique ptr
using TypeErasedSubscriberCallback = std::function<void(std::shared_ptr<MessagePacket>)>;

class TransportSubscriber {
public:
  virtual ~TransportSubscriber() = default;
};

/**
 * @brief SubscriberBase - used to type erase Subscriber
 *
 */
class SubscriberBase {
public:
  virtual ~SubscriberBase() = default;
};

template <typename T_MSG> class Subscriber : public SubscriberBase {
public:
  Subscriber([[maybe_unused]] std::string_view topic, MessageTypeInfo type_info,
             std::vector<std::shared_ptr<TransportSubscriber>> transport_subscribers,
             std::shared_ptr<InprocSubscriber<T_MSG>> inproc)
      : topic(topic), type_info(type_info), transport_subscribers(std::move(transport_subscribers)),
        inproc(std::move(inproc)) {}

  const std::string topic;
  const MessageTypeInfo type_info;
  // TODO: these are shared_ptrs - it could be a single unique_ptr if we were sure we never want to pool these
  std::vector<std::shared_ptr<TransportSubscriber>> transport_subscribers;
  std::shared_ptr<InprocSubscriber<T_MSG>> inproc;

};

} // namespace transport
} // namespace core
} // namespace basis