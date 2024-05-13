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

} // namespace transport
} // namespace core
} // namespace basis