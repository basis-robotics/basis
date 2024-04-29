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
struct MessageEvent<T_MSG> {
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
    std::function<(bool)(MessageEvent<T_MSG> message)> callback;
};

}
}
}