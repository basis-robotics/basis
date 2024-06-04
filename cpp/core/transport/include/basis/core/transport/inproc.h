#pragma once

// TODO: this should probably be pulled out into plugins/?
// TODO: is this header only??
#include <basis/core/transport/message_event.h>
#include <condition_variable>
#include <functional>
#include <memory>
#include <string_view>
#include <typeinfo>
#include <unordered_map>

namespace basis::core::transport {

// This all needs rewritten

// cast to void pointer, pass to function, uncast
// there's no way to preserve type safety here as this will potentially cross shared libraries
/*
We have two choices here:
1. Accept the sadness of void* casts
2. Never allow passing pointers across shared library boundaries
...actually it may be better to just bite the bullet and hash on type_id(T_MSG)::string
but bad things will happen if the implementation changes and you don't recompile.

*/

template <typename T_MSG> struct InprocConnectorInterface {
  virtual void Publish(const std::string &topic, std::shared_ptr<const T_MSG> msg) = 0;
};

// TODO: this can manage its own subscribers, right?
template <typename T_MSG> class InprocPublisher {
public:
  InprocPublisher(std::string_view topic, InprocConnectorInterface<T_MSG> *coordinator)
      : topic(topic), coordinator(coordinator) {}

  // void Publish(const T_MSG &msg) { coordinator->Publish(this->topic, std::make_shared<const T_MSG>(msg)); }
  void Publish(std::shared_ptr<const T_MSG> msg) { coordinator->Publish(this->topic, std::move(msg)); }

private:
  std::string topic;
  InprocConnectorInterface<T_MSG> *coordinator;
};

template <typename T_MSG> class InprocSubscriber {
  // TODO: buffer size
public:
  InprocSubscriber(std::string_view topic_name, const std::function<void(const MessageEvent<T_MSG> &message)> callback) : callback(callback), topic_name(topic_name) {}

  void OnMessage(std::shared_ptr<const T_MSG> msg) {
    MessageEvent<T_MSG> event;
    event.topic_info.topic = topic_name;
    event.message = msg;

    callback(event);
  }

  const std::function<void(MessageEvent<T_MSG> message)> callback;
  const std::string topic_name;
};

// TODO: pass coordinator in, notify on destruction?
template <typename T_MSG> class InprocConnector : public InprocConnectorInterface<T_MSG> {
public:
  // TODO: handle owning the last reference to a publisher/subscriber
  // TODO: ensure if we have one publisher we don't have another of a different type but the same name
  // TODO: ensure type safety for pub/sub
  // TODO: thread safety

  std::shared_ptr<InprocPublisher<T_MSG>> Advertise(std::string_view topic) {
    auto publisher = std::make_shared<InprocPublisher<T_MSG>>(topic, this);
    publishers.insert({std::string{topic}, publisher});
    return publisher;
  }

  std::shared_ptr<InprocSubscriber<T_MSG>> Subscribe(std::string_view topic,
                                                     std::function<void(MessageEvent<T_MSG> message)> callback) {
    auto subscriber = std::make_shared<InprocSubscriber<T_MSG>>(topic, callback);
    subscribers.insert({std::string{topic}, subscriber});
    return subscriber;
  }

private:
  virtual void Publish(const std::string &topic, std::shared_ptr<const T_MSG> msg) {
    auto range = subscribers.equal_range(topic);
    if (range.first == range.second) {
      return;
    }
    for (auto it = range.first; it != range.second; ++it) {
      it->second->OnMessage(msg);
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

  // todo: weak ptr here...?

  std::unordered_map<std::string, std::shared_ptr<InprocPublisher<T_MSG>>> publishers;
  // mutex please
  // todo: weak ptr here...?
  std::unordered_multimap<std::string, std::shared_ptr<InprocSubscriber<T_MSG>>> subscribers;

// WRONG - will lead to differences between compilers
// std::unordered_map<std::string, std::type_info> topic_types;
// this needs to be handled by each class using the transport
// basically - we need to ensure some serialization plugin can handle each thing to ensure no collisions
// using this for raw types is dangerous and needs an ID passed in
#if 0
    // maybe

    // in this case, serialzier would be set to "raw" and id would be set to the type name
    std::shared_ptr<InprocPublisher<T_MSG>> AdvertiseRaw(std::string_view topic, MessageTypeInfo type_info) {

    std::unordered_map<std::string, MessageTypeInfo> topic_types;
#endif
};

class InprocTransport {

private:
  template <typename T> InprocConnector<T> *GetConnector() {
    // TODO: this static somewhat breaks the nice patterns around being explicit about how objects are initialized
    // TODO: test with shared objects
    static InprocConnector<T> connector;
    return &connector;
  }

public:
  template <typename T> std::shared_ptr<InprocPublisher<T>> Advertise(std::string_view topic) {
    return GetConnector<T>()->Advertise(topic);
  }
  template <typename T>
  std::shared_ptr<InprocSubscriber<T>> Subscribe(std::string_view topic,
                                                 std::function<void(MessageEvent<T> message)> callback) {
    return GetConnector<T>()->Subscribe(topic, callback);
  }
};

} // namespace basis::core::transport