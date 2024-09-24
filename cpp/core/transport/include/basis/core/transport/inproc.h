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

struct InprocConnectorBase {

};

template <typename T_MSG> struct InprocConnectorInterface : public InprocConnectorBase {
  virtual void Publish(const std::string &topic, std::shared_ptr<const T_MSG> msg, InprocConnectorBase *ignore_if_primary_connector) = 0;
  virtual bool HasSubscribersFast(const std::string &topic) = 0;
};

// TODO: this can manage its own subscribers, right?
template <typename T_MSG> class InprocPublisher {
public:
  InprocPublisher(std::string_view topic, InprocConnectorInterface<T_MSG> *connector, InprocConnectorBase *ignore_if_primary_connector)
      : topic(topic), connector(connector), ignore_if_primary_connector(ignore_if_primary_connector) {}

  void Publish(std::shared_ptr<const T_MSG> msg) { connector->Publish(this->topic, std::move(msg), ignore_if_primary_connector); }

  bool HasSubscribersFast() { return connector->HasSubscribersFast(topic); }

  InprocConnectorBase *GetConnector() { return connector; }

private:
  std::string topic;
  InprocConnectorInterface<T_MSG> *connector;
  InprocConnectorBase *ignore_if_primary_connector;
};

template <typename T_MSG> class InprocConnector;

template <typename T_MSG> class InprocSubscriber {
  // TODO: buffer size
public:
  InprocSubscriber(std::string_view topic_name, const std::function<void(const MessageEvent<T_MSG> &message)> callback,
                   InprocConnectorBase *connector, InprocConnectorBase* primary_inproc_connector)
      : callback(callback), topic_name(topic_name), connector(connector), primary_inproc_connector(primary_inproc_connector) {}

  void OnMessage(std::shared_ptr<const T_MSG> msg) {
    MessageEvent<T_MSG> event;
    event.topic_info.topic = topic_name;
    event.message = msg;

    callback(event);
  }

  InprocConnectorBase * GetConnector() {
    return connector;
  }

protected:
  friend class InprocConnector<T_MSG>;
  const std::function<void(MessageEvent<T_MSG> message)> callback;
  const std::string topic_name;
  InprocConnectorBase *const connector;
  InprocConnectorBase *const primary_inproc_connector;
};

// TODO: pass coordinator in, notify on destruction?
template <typename T_MSG> class InprocConnector : public InprocConnectorInterface<T_MSG> {
public:
  // TODO: handle owning the last reference to a publisher/subscriber
  // TODO: ensure if we have one publisher we don't have another of a different type but the same name
  // TODO: ensure type safety for pub/sub
  // TODO: thread safety

  std::shared_ptr<InprocPublisher<T_MSG>> Advertise(std::string_view topic, InprocConnectorBase *ignore_if_primary_connector) {
    return std::make_shared<InprocPublisher<T_MSG>>(topic, this, ignore_if_primary_connector);
  }

  std::shared_ptr<InprocSubscriber<T_MSG>> Subscribe(std::string_view topic,
                                                     std::function<void(MessageEvent<T_MSG> message)> callback,
                                                     InprocConnectorBase *primary_inproc_connector) {
    auto subscriber = std::make_shared<InprocSubscriber<T_MSG>>(topic, callback, this, primary_inproc_connector);
    subscribers.insert({std::string{topic}, subscriber});
    return subscriber;
  }

  // Returns true if any subscribers exist. If subscribers are removed they will continue to show until the next
  // Publish() call.
  virtual bool HasSubscribersFast(const std::string &topic) override { return subscribers.contains(topic); }

private:
  virtual void Publish(const std::string &topic, std::shared_ptr<const T_MSG> msg,
                       InprocConnectorBase *ignore_if_primary_connector) override {
    auto range = subscribers.equal_range(topic);

    for (auto it = range.first; it != range.second;) {
      if (auto subscriber = it->second.lock()) {
        if (!ignore_if_primary_connector || subscriber->primary_inproc_connector != ignore_if_primary_connector) {
          subscriber->OnMessage(msg);
        }
        ++it;
      } else {
        subscribers.erase(it++);
      }
    }
  }

  // std::unordered_map<std::string, std::weak_ptr<InprocPublisher<T_MSG>>> publishers;
  //  mutex please

  std::unordered_multimap<std::string, std::weak_ptr<InprocSubscriber<T_MSG>>> subscribers;

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
  template <typename T> InprocConnectorBase *GetConnector() { return GetConnectorInternal<T>(); }

private:
  template <typename T> InprocConnector<T> *GetConnectorInternal() {
    // TODO: this static somewhat breaks the nice patterns around being explicit about how objects are initialized
    // TODO: test with shared objects
    static InprocConnector<T> connector;
    return &connector;
  }

public:
  template <typename T> std::shared_ptr<InprocPublisher<T>> Advertise(std::string_view topic, InprocConnectorBase *ignore_if_primary_connector) {
    return GetConnectorInternal<T>()->Advertise(topic, ignore_if_primary_connector);
  }
  template <typename T>
  std::shared_ptr<InprocSubscriber<T>> Subscribe(std::string_view topic,
                                                 std::function<void(MessageEvent<T> message)> callback, InprocConnectorBase *primary_inproc_connector) {
    return GetConnectorInternal<T>()->Subscribe(topic, callback, primary_inproc_connector);
  }
};

} // namespace basis::core::transport