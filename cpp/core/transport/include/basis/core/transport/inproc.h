#pragma once

#include <basis/core/transport/message_event.h>
#include <condition_variable>
#include <functional>
#include <list>
#include <memory>
#include <string_view>
#include <typeinfo>
#include <unordered_map>

namespace basis::core::transport {

namespace internal {
  // https://en.cppreference.com/w/cpp/container/unordered_map/find
  struct string_hash
  {
      using hash_type = std::hash<std::string_view>;
      using is_transparent = void;
  
      std::size_t operator()(const char* str) const        { return hash_type{}(str); }
      std::size_t operator()(std::string_view str) const   { return hash_type{}(str); }
      std::size_t operator()(std::string const& str) const { return hash_type{}(str); }
  };

}

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
  virtual void Publish(const std::string_view topic, std::shared_ptr<const T_MSG> msg, InprocConnectorBase *ignore_if_primary_connector) = 0;
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

template <typename T_MSG> class InprocConnector : public InprocConnectorInterface<T_MSG> {
  struct SubscribersForTopic {
    bool IsEmpty() {
      std::unique_lock lock(mutex);
      return subscribers.empty();
    }

    std::mutex mutex;
    // This isn't the best data structure for the list, but 
    std::list<std::weak_ptr<InprocSubscriber<T_MSG>>> subscribers;
  };

public:
  // TODO: ensure if we have one publisher we don't have another of a different type but the same name <- this is no longer an error, with separate inproc types
  // TODO: ensure type safety for pub/sub

  std::shared_ptr<InprocPublisher<T_MSG>> Advertise(std::string_view topic, InprocConnectorBase *ignore_if_primary_connector) {
    return std::make_shared<InprocPublisher<T_MSG>>(topic, this, ignore_if_primary_connector);
  }

  std::shared_ptr<InprocSubscriber<T_MSG>> Subscribe(std::string_view topic,
                                                     std::function<void(MessageEvent<T_MSG> message)> callback,
                                                     InprocConnectorBase *primary_inproc_connector) {
    auto subscriber = std::make_shared<InprocSubscriber<T_MSG>>(topic, callback, this, primary_inproc_connector);
    SubscribersForTopic* subscribers = nullptr;
    {
      
      std::unique_lock lock(subscribers_by_topic_mutex);
      auto it = subscribers_by_topic.find(topic);
      if(it == subscribers_by_topic.end()) {
        auto p = subscribers_by_topic.emplace(std::piecewise_construct,
            std::forward_as_tuple(topic),
            std::forward_as_tuple());
        it = p.first;
      }
      subscribers = &it->second;
    }
    subscribers->subscribers.emplace_back(subscriber);

    return subscriber;
  }

  // Returns true if any subscribers exist. If subscribers are removed they will continue to show until the next
  // Publish() call.
  virtual bool HasSubscribersFast(const std::string &topic) override { 
    std::unique_lock lock(subscribers_by_topic_mutex);
    auto it = subscribers_by_topic.find(topic);
    return it != subscribers_by_topic.end() && !it->second.IsEmpty();
  }

private:
  virtual void Publish([[maybe_unused]] const std::string_view topic, std::shared_ptr<const T_MSG> msg,
                       InprocConnectorBase *ignore_if_primary_connector) override {
    // First get the subscriber
    SubscribersForTopic* subscribers = nullptr;
    {
      std::unique_lock lock(subscribers_by_topic_mutex);
      auto it = subscribers_by_topic.find(topic);
      if(it == subscribers_by_topic.end()) {
        return;
      }
      subscribers = &it->second;
    }

    {
      std::vector<std::shared_ptr<InprocSubscriber<T_MSG>>> valid_subscribers;

      std::unique_lock lock(subscribers->mutex);
      valid_subscribers.reserve(subscribers->subscribers.size());
      auto it = subscribers->subscribers.begin();
      while(it != subscribers->subscribers.end()) {
        if (auto subscriber = it->lock()) {
          if (!ignore_if_primary_connector || subscriber->primary_inproc_connector != ignore_if_primary_connector) {
            valid_subscribers.push_back(subscriber);
          } 
          ++it;
        } else {
          it = subscribers->subscribers.erase(it);
        }
      }
      for(auto& subscriber : valid_subscribers) {
        subscriber->OnMessage(msg);
      }
    }
  }


  std::mutex subscribers_by_topic_mutex;

  std::unordered_map<std::string, SubscribersForTopic, internal::string_hash, std::equal_to<>> subscribers_by_topic;

// This might be wrong if compiled with different compilers.
// std::unordered_map<std::string, std::type_info> topic_types;
// this needs to be handled by each class using the transport
// basically - we need to ensure some serialization plugin can handle each thing to ensure no collisions
// using this for raw types is dangerous and needs an ID passed in
};

class InprocTransport {
  template <typename T> InprocConnectorBase *GetConnector() { return GetConnectorInternal<T>(); }

private:
  template <typename T> InprocConnector<T> *GetConnectorInternal() {
    // TODO: this static somewhat breaks the nice patterns around being explicit about how objects are initialized
    static InprocConnector<T> connector;
    return &connector;
  }

public:
  template <typename T> std::shared_ptr<InprocPublisher<T>> Advertise(std::string_view topic, InprocConnectorBase *ignore_if_primary_connector) {
    return GetConnectorInternal<T>()->Advertise(topic, ignore_if_primary_connector);
  }
  template <typename T>
  std::shared_ptr<InprocSubscriber<T>> Subscribe(const std::string_view topic,
                                                 std::function<void(MessageEvent<T> message)> callback, InprocConnectorBase *primary_inproc_connector) {
    return GetConnectorInternal<T>()->Subscribe(topic, callback, primary_inproc_connector);
  }
};

} // namespace basis::core::transport