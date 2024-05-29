#pragma once

#include <memory>
#include <span>

#include <spdlog/spdlog.h>

#include "inproc.h"
#include "message_type_info.h"
#include "publisher.h"
#include "publisher_info.h"
#include "subscriber.h"
#include "thread_pool_manager.h"

#include "simple_mpsc.h"

#include <basis/core/serialization.h>

namespace basis::core::transport {

/**
 * Helper for holding incomplete messages.
 */
class IncompleteMessagePacket {
public:
  IncompleteMessagePacket() = default;

  std::span<std::byte> GetCurrentBuffer() {
    if (incomplete_message) {
      std::span<std::byte> ret = incomplete_message->GetMutablePayload();
      return ret.subspan(progress_counter);
    } else {
      return std::span<std::byte>(incomplete_header + progress_counter, sizeof(MessageHeader) - progress_counter);
    }
  }

  bool AdvanceCounter(size_t amount) {
    progress_counter += amount;
    if (!incomplete_message && progress_counter == sizeof(MessageHeader)) {
      // todo: check for header validity here
      progress_counter = 0;
      incomplete_message = std::make_unique<MessagePacket>(completed_header);
    }
    if (!incomplete_message) {
      return false;
    }
    return progress_counter == incomplete_message->GetMessageHeader()->data_size;
  }

  std::unique_ptr<MessagePacket> GetCompletedMessage() {
    // assert(incomplete_message);
    // assert(progress_counter == incomplete_message->GetMessageHeader()->data_size);
    progress_counter = 0;
    return std::move(incomplete_message);
  }

  size_t GetCurrentProgress() { return progress_counter; }

private:
  union {
    std::byte incomplete_header[sizeof(MessageHeader)] = {};
    MessageHeader completed_header;
  };
  std::unique_ptr<MessagePacket> incomplete_message;

  size_t progress_counter = 0;
};

// TODO: use MessageEvent
// TODO: don't store the packet directly, store a weak reference to the transport subscriber
struct OutputQueueEvent {
  std::string topic_name;
  std::unique_ptr<MessagePacket> packet;
  TypeErasedSubscriberCallback callback;
};
using OutputQueue = SimpleMPSCQueue<OutputQueueEvent>;

class Transport {
public:
  Transport(std::shared_ptr<basis::core::transport::ThreadPoolManager> thread_pool_manager)
      : thread_pool_manager(thread_pool_manager) {}
  virtual ~Transport() = default;
  virtual std::shared_ptr<TransportPublisher> Advertise(std::string_view topic, MessageTypeInfo type_info) = 0;
  virtual std::shared_ptr<TransportSubscriber> Subscribe(std::string_view topic, TypeErasedSubscriberCallback callback,
                                                         OutputQueue *output_queue, MessageTypeInfo type_info) = 0;

  /**
   * Implementations should call this function at a regular rate.
   * @todo: do we want to keep this or enforce each transport taking care of its own update calls?
   */
  virtual void Update() {}

protected:
  /// Thread pools are shared across transports
  std::shared_ptr<basis::core::transport::ThreadPoolManager> thread_pool_manager;
};

// todo: break this into a separate library - transports don't need to know about it
class TransportManager {
public:
  TransportManager(std::unique_ptr<InprocTransport> inproc = nullptr) : inproc(std::move(inproc)) {}
  // todo: deducing a raw type should be an error unless requested
  template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
  std::shared_ptr<Publisher<T_MSG>> Advertise(std::string_view topic,
                                              MessageTypeInfo message_type = DeduceMessageTypeInfo<T_MSG>()) {

    std::shared_ptr<InprocPublisher<T_MSG>> inproc_publisher;
    if (inproc) {
      inproc_publisher = inproc->Advertise<T_MSG>(topic);
    }
    std::vector<std::shared_ptr<TransportPublisher>> tps;
    for (auto &[transport_name, transport] : transports) {
      tps.push_back(transport->Advertise(topic, message_type));
    }

    SerializeGetSizeCallback<T_MSG> get_size_cb = T_Serializer::template GetSerializedSize<T_MSG>;
    SerializeWriteSpanCallback<T_MSG> write_span_cb = T_Serializer::template SerializeToSpan<T_MSG>;

    auto publisher = std::make_shared<Publisher<T_MSG>>(topic, message_type, std::move(tps), inproc_publisher,
                                                        std::move(get_size_cb), std::move(write_span_cb));
    publishers.emplace(std::string(topic), publisher);
    return publisher;
  }

  template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
  std::shared_ptr<Subscriber<T_MSG>> Subscribe(std::string_view topic, SubscriberCallback<T_MSG> callback,
                                               core::transport::OutputQueue *output_queue = nullptr,
                                               MessageTypeInfo message_type = DeduceMessageTypeInfo<T_MSG>()) {
    std::shared_ptr<InprocSubscriber<T_MSG>> inproc_subscriber;

    [[maybe_unused]] TypeErasedSubscriberCallback outer_callback = [topic,
                                                                    callback](std::shared_ptr<MessagePacket> packet) {
      std::shared_ptr<const T_MSG> message = T_Serializer::template DeserializeFromSpan<T_MSG>(packet->GetPayload());
      if (!message) {
        spdlog::error("Unable to deserialize message on topic {}", topic);
        return;
      }
      callback(std::move(message));
    };

    if (inproc) {
#if 0
      inproc_subscriber = inproc->Subscribe<T>(topic, [](MessageEvent<T_MSG>){});
        // TODO
#endif
    }

    std::vector<std::shared_ptr<TransportSubscriber>> tps;

    for (auto &[transport_name, transport] : transports) {
      tps.push_back(transport->Subscribe(topic, outer_callback, output_queue, message_type));
    }

    auto subscriber = std::make_shared<Subscriber<T_MSG>>(topic, message_type, std::move(tps), inproc_subscriber);
    subscribers.emplace(std::string(topic), subscriber);

    if(use_local_publishers_for_subscribers) {
      subscriber->HandlePublisherInfo(GetLastPublisherInfo());
    }
    subscriber->HandlePublisherInfo(last_network_publish_info[std::string(topic)]);

    return subscriber;
  }


  /**
   *
   * @todo error handling, fail if there's already one of the same name
   * @todo can we ask the transport name from the transport?
   */
  void RegisterTransport(std::string_view transport_name, std::unique_ptr<Transport> transport) {
    transports.emplace(std::string(transport_name), std::move(transport));
  }

  void Update() {
    for (auto &[_, transport] : transports) {
      transport->Update();
    }

    // Generate updated topic info and clean up old publishers
    std::vector<PublisherInfo> new_publisher_info;

    for (auto it = publishers.cbegin(); it != publishers.cend();)
    {
      if(auto publisher = it->second.lock())
      {
        new_publisher_info.emplace_back(publisher->GetPublisherInfo());
        ++it;
      }
      else
      {
        it = publishers.erase(it);
      }
    }

    last_owned_publish_info = std::move(new_publisher_info);

  }

  const std::vector<PublisherInfo>& GetLastPublisherInfo() { return last_owned_publish_info; }

   
    basis::core::transport::proto::TransportManagerInfo GetTransportManagerInfo() {
      /// @todo arena allocate
      basis::core::transport::proto::TransportManagerInfo sent_info;
      for(auto& pub_info : last_owned_publish_info) {
        *sent_info.add_publishers() = pub_info.ToProto();
      }
      return sent_info;
    }

  void HandleNetworkInfo(const proto::NetworkInfo& network_info) {
    last_network_publish_info.clear();

    for(auto& [topic, publisher_infos_msg] : network_info.publishers_by_topic()) {
      std::vector<PublisherInfo>& info = last_network_publish_info[topic];
      info.reserve(publisher_infos_msg.publishers_size());
      for(auto& publisher_info : publisher_infos_msg.publishers()) {
        info.emplace_back(PublisherInfo::FromProto(publisher_info));
      }
      for(auto [it, end] = subscribers.equal_range(topic); it != end; it++) { 
        auto subscriber = it->second.lock();
        if(subscriber) {
          subscriber->HandlePublisherInfo(info);
        }
      }
    }
  }

protected:
  /// @todo id? probably not needed, pid is fine, unless we _really_ need multiple transport managers
  /// ...which might be needed for integration testing

  /**
   * publisher summary from last Update() call
   */
  std::vector<PublisherInfo> last_owned_publish_info;

  std::unordered_map<std::string, std::vector<PublisherInfo>> last_network_publish_info;
  
  std::unique_ptr<InprocTransport> inproc;

  std::unordered_map<std::string, std::unique_ptr<Transport>> transports;

  std::unordered_multimap<std::string, std::weak_ptr<PublisherBase>> publishers;

  /**
   * The subscribers.
   * 
   * @todo: it may be wise to make this a unnordered_map<string, vector<subscriber>> instead
   */
  std::unordered_multimap<std::string, std::weak_ptr<SubscriberBase>> subscribers;

  /**
   * For testing only - set to false to disable using known subscribers and force a coordinator connection
   */
  bool use_local_publishers_for_subscribers = true;
};

} // namespace basis::core::transport