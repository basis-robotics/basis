#pragma once

#include <memory>
#include <span>

#include <spdlog/spdlog.h>

#include "inproc.h"
#include "publisher.h"
#include "publisher_info.h"
#include "subscriber.h"

#include <basis/core/serialization.h>

#include "transport.h"

namespace basis::core::transport {

class SchemaManager {
public:
  SchemaManager() {}

  template <typename T_MSG, typename T_Serializer> void RegisterType(const serialization::MessageTypeInfo &type_info) {
    const std::string schema_id = type_info.SchemaId();
    if (!known_schemas.contains(schema_id)) {
      known_schemas.insert(schema_id);

      if constexpr (!std::is_same_v<T_Serializer, serialization::RawSerializer>) {
        schemas_to_send.push_back(T_Serializer::template DumpSchema<T_MSG>());
      }
    }
  }

  std::vector<serialization::MessageSchema> &&ConsumeSchemasToSend() { return std::move(schemas_to_send); }

protected:
  std::unordered_set<std::string> known_schemas;

  std::vector<serialization::MessageSchema> schemas_to_send;
};

/**
 * Class responsible for creating publishers/subscribers and accumulating data to send to the Coordinator
 */
class TransportManager {
public:
  TransportManager(std::unique_ptr<InprocTransport> inproc = nullptr) : inproc(std::move(inproc)) {}
  // todo: deducing a raw type should be an error unless requested
  template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
  [[nodiscard]] std::shared_ptr<Publisher<T_MSG>>
  Advertise(std::string_view topic,
            serialization::MessageTypeInfo message_type = T_Serializer::template DeduceMessageTypeInfo<T_MSG>()) {
    schema_manager.RegisterType<T_MSG, T_Serializer>(message_type);

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

  /**
   * Subscribe to a topic, without attempting to deserialize.
   *
   * @warning This will not subscribe to messages sent over the `inproc` transport, as they are not serialized in the
   * first place.
   */
  [[nodiscard]] std::shared_ptr<SubscriberBase> SubscribeRaw(std::string_view topic,
                                                             TypeErasedSubscriberCallback callback,
                                                             basis::core::threading::ThreadPool *work_thread_pool,
                                                             core::transport::OutputQueue *output_queue,
                                                             serialization::MessageTypeInfo message_type) {
    return SubscribeInternal<SubscriberBase>(topic, callback, work_thread_pool, output_queue, message_type, {});
  }

  template <typename T_MSG, typename T_Serializer = SerializationHandler<T_MSG>::type>
  [[nodiscard]] std::shared_ptr<Subscriber<T_MSG>>
  Subscribe(std::string_view topic, SubscriberCallback<T_MSG> callback,
            basis::core::threading::ThreadPool *work_thread_pool,
            core::transport::OutputQueue *output_queue = nullptr,
            serialization::MessageTypeInfo message_type = T_Serializer::template DeduceMessageTypeInfo<T_MSG>()) {
    std::shared_ptr<InprocSubscriber<T_MSG>> inproc_subscriber;

    TypeErasedSubscriberCallback outer_callback = [topic, callback](std::shared_ptr<MessagePacket> packet) {
      std::shared_ptr<const T_MSG> message = T_Serializer::template DeserializeFromSpan<T_MSG>(packet->GetPayload());
      if (!message) {
        // todo: change the callback to take the topic as well?
        spdlog::error("Unable to deserialize message on topic {}", topic);
        return;
      }
      callback(std::move(message));
    };

    if (inproc) {
      inproc_subscriber = inproc->Subscribe<T_MSG>(topic, [output_queue, callback](MessageEvent<T_MSG> msg) {
        if(output_queue) {

          output_queue->Emplace(
            [callback = callback, message = msg.message]()
            {
              callback(message);
            }
            );
        }
        else {
          callback(std::move(msg.message));
        }

      });

    }

    return SubscribeInternal<Subscriber<T_MSG>>(topic, outer_callback, work_thread_pool, output_queue, message_type, inproc_subscriber);
  }

  /**
   *
   * @todo error handling, fail if there's already one of the same name
   * @todo can we ask the transport name from the transport?
   */
  void RegisterTransport(std::string_view transport_name, std::unique_ptr<Transport> transport) {
    transports.emplace(std::string(transport_name), std::move(transport));
  }

  /**
   * Updates all transports and cleans up old publishers.
   */
  void Update() {
    for (auto &[_, transport] : transports) {
      transport->Update();
    }

    // Generate updated topic info and clean up old publishers
    std::vector<PublisherInfo> new_publisher_info;

    for (auto it = publishers.cbegin(); it != publishers.cend();) {
      if (auto publisher = it->second.lock()) {
        new_publisher_info.emplace_back(publisher->GetPublisherInfo());
        ++it;
      } else {
        it = publishers.erase(it);
      }
    }

    last_owned_publish_info = std::move(new_publisher_info);
  }

  const std::vector<PublisherInfo> &GetLastPublisherInfo() { return last_owned_publish_info; }

  basis::core::transport::proto::TransportManagerInfo GetTransportManagerInfo() {
    /// @todo arena allocate
    basis::core::transport::proto::TransportManagerInfo sent_info;
    for (auto &pub_info : last_owned_publish_info) {
      *sent_info.add_publishers() = pub_info.ToProto();
    }
    return sent_info;
  }

  void HandleNetworkInfo(const proto::NetworkInfo &network_info) {
    last_network_publish_info.clear();

    for (auto &[topic, publisher_infos_msg] : network_info.publishers_by_topic()) {
      std::vector<PublisherInfo> &info = last_network_publish_info[topic];
      info.reserve(publisher_infos_msg.publishers_size());
      for (auto &publisher_info : publisher_infos_msg.publishers()) {
        info.emplace_back(PublisherInfo::FromProto(publisher_info));
      }
      for (auto [it, end] = subscribers.equal_range(topic); it != end; it++) {
        auto subscriber = it->second.lock();
        if (subscriber) {
          subscriber->HandlePublisherInfo(info);
        }
      }
    }
  }

  SchemaManager &GetSchemaManager() { return schema_manager; }

protected:
  /**
   * Internal helper used to allow subscribing with a number of different callback signatures.
   *
   * @tparam T_SUBSCRIBER the subscriber type in use - typically Subscriber<MyMessageType> or SubscriberBase for raw
   * @tparam T_INPROC_SUBSCRIBER
   * @param topic the topic to subscribe to
   * @param callback type independent callback
   * @param output_queue
   * @param message_type
   * @param inproc_subscriber can be nullptr for SubscriberBase
   * @return std::shared_ptr<T_SUBSCRIBER>
   */
  template <typename T_SUBSCRIBER, typename T_INPROC_SUBSCRIBER = void>
  [[nodiscard]] std::shared_ptr<T_SUBSCRIBER>
  SubscribeInternal(std::string_view topic, TypeErasedSubscriberCallback callback, basis::core::threading::ThreadPool *work_thread_pool,
                    core::transport::OutputQueue *output_queue, serialization::MessageTypeInfo message_type,
                    std::shared_ptr<T_INPROC_SUBSCRIBER> inproc_subscriber) {

    std::vector<std::shared_ptr<TransportSubscriber>> tps;

    for (auto &[transport_name, transport] : transports) {
      tps.push_back(transport->Subscribe(topic, callback, work_thread_pool, output_queue, message_type));
    }

    std::shared_ptr<T_SUBSCRIBER> subscriber;
    if constexpr (std::is_same_v<T_SUBSCRIBER, SubscriberBase>) {
      subscriber = std::make_shared<T_SUBSCRIBER>(topic, message_type, std::move(tps), false);
    } else {
      subscriber = std::make_shared<T_SUBSCRIBER>(topic, message_type, std::move(tps), inproc_subscriber);
    }
    subscribers.emplace(std::string(topic), subscriber);

    if (use_local_publishers_for_subscribers) {
      subscriber->HandlePublisherInfo(GetLastPublisherInfo());
    }
    subscriber->HandlePublisherInfo(last_network_publish_info[std::string(topic)]);

    return subscriber;
  }
  /// @todo id? probably not needed, pid is fine, unless we _really_ need multiple transport managers
  /// ...which might be needed for integration testing

  /**
   * Publisher summary from last Update() call
   */
  std::vector<PublisherInfo> last_owned_publish_info;

  /**
   * Latest summary of publishers from other processes.
   */
  std::unordered_map<std::string, std::vector<PublisherInfo>> last_network_publish_info;

  /**
   * The inproc transport. Optional as for testing sending shared pointers directly may not be desired.
   * This is separate from `transports` as it has a different API - InprocTransport is type safe, and doesn't need to
   * (de)serialize.
   */
  std::unique_ptr<InprocTransport> inproc;

  /**
   * A map of transport ID to transport.
   */
  std::unordered_map<std::string, std::unique_ptr<Transport>> transports;

  /**
   * The publishers we've created.
   */
  std::unordered_multimap<std::string, std::weak_ptr<PublisherBase>> publishers;

  /**
   * The subscribers we've created.
   *
   * @todo: it may be wise to make this a unnordered_map<string, vector<subscriber>> instead
   */
  std::unordered_multimap<std::string, std::weak_ptr<SubscriberBase>> subscribers;

  SchemaManager schema_manager;

  /**
   * For testing only - set to false to disable using known subscribers and force a coordinator connection
   */
  bool use_local_publishers_for_subscribers = true;
};

} // namespace basis::core::transport