#pragma once

// TODO: this should probably be pulled out into plugins/?

#include <memory>
#include <string_view>
#include <typeinfo>
#include <unordered_map>
#include <basis/core/transport/publisher.h>
#include <basis/core/transport/subscriber.h>

namespace basis {
namespace core {
namespace transport {

// cast to void pointer, pass to function, uncast
// there's no way to preserve type safety here as this will potentially cross shared libraries
/*
We have two choices here:
1. Accept the sadness of void* casts
2. Never allow passing pointers across shared library boundaries
...actually it may be better to just bite the bullet and hash on type_id(T_MSG)::string
but bad things will happen if the implementation changes and you don't recompile.

*/
struct InprocCoordinatorInterface {
    template<typename T_MSG>
    void Publish(const std::string& topic, const T_MSG& data) {
        // TODO: optimize copy away if we have no subscribers
        auto shared = std::make_shared<T_MSG>(data);
        PublishRawPtr(topic, &shared, sizeof(T_MSG));
    }

protected:
    virtual void PublishRawPtr(const std::string& topic, const void* data, size_t size) = 0;
};


template<typename T_MSG>
class InprocPublisher : public PublisherBaseT<T_MSG> {
public:
    InprocPublisher(std::string_view topic, InprocCoordinatorInterface* coordinator) : topic(topic), coordinator(coordinator) {}

    virtual void Publish(const T_MSG& data) override {
        coordinator->Publish(this->topic, data);
    }

private:
    std::string topic;
    InprocCoordinatorInterface* coordinator;
};

template<typename T_MSG>
class InprocSubscriber : public SubscriberBaseT<T_MSG> {
public:
    InprocSubscriber(const std::function<void(const MessageEvent<T_MSG>& message)> callback) : SubscriberBaseT<T_MSG>(callback) {}

    virtual void OnRawPointer(const void* data, size_t size) override {
        MessageEvent<T_MSG> event;
        event.message = *reinterpret_cast<const std::shared_ptr<T_MSG>*>(data);
        this->callback(event);
    }
};



// TODO: pass coordinator in, notify on destruction?

class InprocCoordinator : public InprocCoordinatorInterface {
public:
    // TODO: handle owning the last reference to a publisher/subscriber
    // TODO: ensure if we have one publisher we don't have another of a different type but the same name
    // TODO: ensure type safety for pub/sub

    template<typename T_MSG>
    std::shared_ptr<PublisherBaseT<T_MSG>> Advertise(std::string_view topic) {
        auto publisher = std::make_shared<InprocPublisher<T_MSG>>(topic, this);
        publishers.insert({std::string{topic}, publisher});
        return publisher;
    }

    template<typename T_MSG>
    std::unique_ptr<SubscriberBaseT<T_MSG>> Subscribe(std::string_view topic, std::function<void(MessageEvent<T_MSG> message)> callback) {
        auto subscriber = std::make_shared<InprocSubscriber<T_MSG>>(callback);
        subscribers.insert({std::string{topic}, subscriber});
        return nullptr;
    }


private:
    virtual void PublishRawPtr(const std::string& topic, const void* data, size_t size) {
        printf("Publishing\n");
        auto range = subscribers.equal_range(topic);
        if(range.first == range.second) {
            return;
        }
        for (auto it = range.first; it != range.second; ++it) {
            it->second->OnRawPointer(data, size);
        }
    }

/*
    template<typename T_MSG>
    void Publish(const std::string& topic, const T_MSG& data) {
        auto range = subscribers.equal_range(topic);
        for (auto it = range.first; it != range.second; ++it) {
            auto subscriber = std::static_pointer_cast<SubscriberBaseT<T_MSG>>(it->second);
            subscriber->callback(MessageEvent<T_MSG>{});
        }
    }
*/
    std::unordered_map<std::string, std::shared_ptr<PublisherBase>> publishers;
    // mutex please

    std::unordered_multimap<std::string, std::shared_ptr<SubscriberBase>> subscribers;

    // WRONG - will lead to differences between compilers
        //std::unordered_map<std::string, std::type_info> topic_types;
    // this needs to be handled by each class using the transport
    // basically - we need to ensure some serialization plugin can handle each thing to ensure no collisions
    // using this for raw types is dangerous and needs an ID passed in
    #if 0
    // maybe

    struct MessageTypeInfo {
        std::string serializer;
        std::string id;
        size_t type_size = 0; // Required for raw types, to help ensure safety
    };

    // in this case, serialzier would be set to "raw" and id would be set to the type name
    std::shared_ptr<PublisherBaseT<T_MSG>> AdvertiseRaw(std::string_view topic, MessageTypeInfo type_info) {

    std::unordered_map<std::string, MessageTypeInfo> topic_types;
        #endif

};

}
}
}