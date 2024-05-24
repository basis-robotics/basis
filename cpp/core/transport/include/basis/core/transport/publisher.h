#pragma once
#include <cassert>
#include <memory>
#include <string_view>

#include "inproc.h"
#include "message_packet.h"
#include "message_type_info.h"

#include <basis/core/serialization.h>
namespace basis::core::transport {
/**
 * @brief PublisherBase - used to type erase Publisher
 *
 */
class PublisherBase {
public:
  PublisherBase() = default;
  virtual ~PublisherBase() = default;
};

class TransportPublisher {
public:
  virtual ~TransportPublisher() = default;

  virtual void SendMessage(std::shared_ptr<MessagePacket> message) = 0;

  virtual std::string GetPublisherInfo() = 0;

  virtual size_t GetSubscriberCount() = 0;
};

template <typename T_MSG> class Publisher : public PublisherBase {
public:
  Publisher(std::string_view topic, MessageTypeInfo type_info,
            std::vector<std::shared_ptr<TransportPublisher>> transport_publishers,
            std::shared_ptr<InprocPublisher<T_MSG>> inproc, SerializeGetSizeCallback<T_MSG> get_message_size_cb,
            SerializeWriteSpanCallback<T_MSG> write_message_to_span_cb)

      : topic(topic), type_info(type_info), inproc(inproc), transport_publishers(std::move(transport_publishers)),
        get_message_size_cb(std::move(get_message_size_cb)),
        write_message_to_span_cb(std::move(write_message_to_span_cb)) {}

  std::vector<std::string> GetPublisherInfo() {
    std::vector<std::string> out;
    for (auto &pub : transport_publishers) {
      out.push_back(pub->GetPublisherInfo());
    }
    return out;
  }

  size_t GetSubscriberCount() {
    size_t n = 0;
    for (auto &transport_publisher : transport_publishers) {
      n += transport_publisher->GetSubscriberCount();
    }
    return n;
  }

  virtual void Publish(std::shared_ptr<const T_MSG> msg) {
    assert(type_info.serializer == "raw");

    if (inproc) {
      inproc->Publish(msg);
    }

    // TODO: early out if there are no transport subscribers, to avoid serialization
    // TODO: if the cost of serialization is high, it may be good to move the work onto a different thread

    // Serialize

    // Request size of payload from serializer
    const size_t payload_size = get_message_size_cb(*msg);
    // Create a packet of the proper size
    auto packet = std::make_shared<MessagePacket>(MessageHeader::DataType::MESSAGE, payload_size);
    // Serialize directly to the packet
    std::span<std::byte> payload = packet->GetMutablePayload();
    if(!write_message_to_span_cb(*msg, payload)) {
      spdlog::error("Unable to serialize message on topic {}", topic);
      return;
    }

    // Send the data
    for (auto &pub : transport_publishers) {
      pub->SendMessage(packet);
    }
  }

  const std::string topic;
  const MessageTypeInfo type_info;
  std::shared_ptr<InprocPublisher<T_MSG>> inproc;
  // TODO: these are shared_ptrs - it could be a single unique_ptr if we were sure we never want to pool these
  std::vector<std::shared_ptr<TransportPublisher>> transport_publishers;
  SerializeGetSizeCallback<T_MSG> get_message_size_cb;
  SerializeWriteSpanCallback<T_MSG> write_message_to_span_cb;
};

} // namespace basis::core::transport