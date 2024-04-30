#include <functional>
#include <memory>
#include <basis/core/time.h>
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

    // TODO: this might not belong here at all, only needed by inproc?
    virtual void OnRawPointer(const void* data, size_t size) = 0;
};

struct TopicInfo {
    // TODO: a bunch of allocations here
    std::string topic;
    std::string type;
    std::string publisher_unit;
    std::string publisher_host;
};

template<typename T_MSG> 
struct MessageEvent {
    Time time;
    TopicInfo topic_info;
    
    std::shared_ptr<T_MSG> message;
};

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
    std::function<void(MessageEvent<T_MSG> message)> callback;
};

}
}
}