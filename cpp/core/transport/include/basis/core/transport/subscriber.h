#pragma once
#include <basis/core/time.h>
#include <basis/core/transport/message_event.h>
#include <basis/core/transport/message_type_info.h>
#include <basis/core/transport/inproc.h>

#include <functional>
#include <memory>
namespace basis {
namespace core {
namespace transport {

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