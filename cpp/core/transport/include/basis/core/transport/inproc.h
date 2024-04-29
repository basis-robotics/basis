#pragma once
#include <basis/core/transport/publisher.h>
#include <basis/core/transport/subscriber.h>

namespace basis {
namespace core {
namespace transport {

template<typename T_MSG>
class InprocSubscriber : public SubscriberBaseT<T_MSG> {

};


template<typename T_MSG>
class InprocPublisher : public PublisherBaseT<T_MSG> {
public:

    virtual void Publish(const T_MSG& data) override {

    }
};

// TODO: pass coordinator in, notify on destruction?

class InprocCoordinator {
public:
// perhaps shared_ptr? and weak_ on the others?
    template<typename T_MSG>
    std::unique_ptr<SubscriberBase<T_MSG>> Publish(std::string_view topic) {
        auto publisher = std::make_unique<InprocPublisher<T_MSG>>();
        publishers[topic] = publisher;
        return std::move(publisher);
    }

    template<typename T_MSG>
    std::unique_ptr<SubscriberBase<T_MSG>> Subscribe(std::string_view topic, std::function<(void)(MessageEvent<T_MSG> message)> callback) {
        auto subscriber = std::make_unique<InprocPublisher<T_MSG>>(callback);
        subscribers[topic] = subscriber;
        return std::move(subscriber);
    }

private:

    std::unordered_map<std::string, PublisherBase*>> publishers;
    std::unordered_map<std::string, SubscriberBase*>> subscribers;

}
     

}
}
}