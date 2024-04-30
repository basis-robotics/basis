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
    
    std::shared_ptr<const T_MSG> message;
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

    virtual void OnMessage(std::shared_ptr<const T_MSG> msg) = 0;

    std::function<void(MessageEvent<T_MSG> message)> callback;
};

}
}
}