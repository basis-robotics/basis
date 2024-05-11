#pragma once
#include <basis/core/time.h>
#include <basis/core/transport/message_event.h>
#include <functional>
#include <memory>
namespace basis {
namespace core {
namespace transport {

/**
 * @brief SubscriberBase - used to type erase Subscriber
 *
 */
class SubscriberBase {
public:
  virtual ~SubscriberBase() = default;
};
#if 0
/**
 * @brief SubscriberBaseT
 * 
 * @tparam T_MSG - sererializable type to be subscribed to
 */
template<typename T_MSG>
class SubscriberBaseT : public SubscriberBase {
public:
    // TODO: this forces callbacks to be copyable
    SubscriberBaseT(const std::function<void(const MessageEvent<T_MSG>& message)> callback) : callback(std::move(callback)) {}

    virtual void OnMessage(std::shared_ptr<const T_MSG> msg) = 0;

    virtual void ConsumeMessages(bool wait = false) = 0;

    std::function<void(MessageEvent<T_MSG> message)> callback;
};

#endif
} // namespace transport
} // namespace core
} // namespace basis