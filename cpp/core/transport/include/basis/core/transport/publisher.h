#pragma once
#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string_view>

#include <spdlog/spdlog.h>

#include "basis/core/time.h"
#include "inproc.h"
#include "logger.h"
#include "message_packet.h"
#include "publisher_info.h"

#include <basis/core/serialization.h>
#include <basis/core/serialization/message_type_info.h>

#include <basis/recorder.h>

#include "convertable_inproc.h"

namespace basis::core::transport {

extern std::atomic<uint32_t> publisher_id_counter;

/**
 * Create a publisher ID per Publisher.
 */
__uint128_t CreatePublisherId();

class TransportPublisher {
public:
  virtual ~TransportPublisher() = default;

  virtual void SendMessage(std::shared_ptr<MessagePacket> message) = 0;

  virtual std::string GetTransportName() = 0;

  virtual std::string GetConnectionInformation() = 0;

  virtual size_t GetSubscriberCount() = 0;

  virtual void SetMaxQueueSize(size_t max_queue_size) = 0;
};

/**
 * @brief PublisherBase - used to type erase Publisher
 */
class PublisherBase {
protected:
  PublisherBase(std::string_view topic, serialization::MessageTypeInfo type_info, bool has_inproc,
                std::vector<std::shared_ptr<TransportPublisher>> transport_publishers, RecorderInterface *recorder)
      : topic(topic), type_info(type_info), has_inproc(has_inproc), transport_publishers(transport_publishers),
        recorder(recorder) {}

public:
  virtual ~PublisherBase() = default;

  PublisherInfo GetPublisherInfo();

  void SetMaxQueueSize(size_t max_queue_size) {
    for (auto &pub : transport_publishers) {
      pub->SetMaxQueueSize(max_queue_size);
    }
  }

protected:
  void PublishRaw(std::shared_ptr<MessagePacket> packet, basis::core::MonotonicTime now) {
    // Send the data
    for (auto &pub : transport_publishers) {
      pub->SendMessage(packet);
    }
    if (recorder) {
      recorder->WriteMessage(topic, {packet, packet->GetPayload()}, now);
    }
  }
  const __uint128_t publisher_id = CreatePublisherId();
  const std::string topic;
  const serialization::MessageTypeInfo type_info;
  const bool has_inproc;
  // TODO: these are shared_ptrs - it could be a single unique_ptr if we were sure we never want to pool these
  std::vector<std::shared_ptr<TransportPublisher>> transport_publishers;
  RecorderInterface *recorder;
};

class PublisherRaw : public PublisherBase {
public:
  PublisherRaw(std::string_view topic, serialization::MessageTypeInfo type_info,
               std::vector<std::shared_ptr<TransportPublisher>> transport_publishers, RecorderInterface *recorder)
      : PublisherBase(topic, type_info, false, std::move(transport_publishers), recorder) {}

  using PublisherBase::PublishRaw;
};

template <typename T_MSG, typename T_CONVERTABLE_INPROC = NoAdditionalInproc> class Publisher : public PublisherBase {
public:
  Publisher(std::string_view topic, serialization::MessageTypeInfo type_info,
            std::vector<std::shared_ptr<TransportPublisher>> transport_publishers,
            std::shared_ptr<InprocPublisher<T_MSG>> inproc, SerializeGetSizeCallback<T_MSG> get_message_size_cb,
            SerializeWriteSpanCallback<T_MSG> write_message_to_span_cb, basis::RecorderInterface *recorder = nullptr,
            std::shared_ptr<InprocPublisher<T_CONVERTABLE_INPROC>> convertable_inproc = nullptr)
      : PublisherBase(topic, type_info, inproc != nullptr, transport_publishers, recorder), inproc(inproc),
        convertable_inproc(convertable_inproc), get_message_size_cb(std::move(get_message_size_cb)),
        write_message_to_span_cb(std::move(write_message_to_span_cb)) {}

  size_t GetTransportSubscriberCount() {
    size_t n = 0;
    for (auto &transport_publisher : transport_publishers) {
      n += transport_publisher->GetSubscriberCount();
    }
    return n;
  }

  virtual void Publish(std::shared_ptr<const T_CONVERTABLE_INPROC> msg) {
    if constexpr (!std::is_same_v<T_CONVERTABLE_INPROC, NoAdditionalInproc>) {
      assert(convertable_inproc);
      convertable_inproc->Publish(msg);

      if (GetTransportSubscriberCount() > 0 || inproc->HasSubscribersFast()) {
        // This can someday be made async
        Publish(ConvertToMessage<T_MSG>(msg));
      }
    }
  }

  virtual void Publish(std::shared_ptr<const T_MSG> msg) {
    if (inproc) {
      inproc->Publish(msg);
    }

    if (!GetTransportSubscriberCount()) {
      return;
    }

    // TODO: if the cost of serialization is high, it may be good to move the work onto a different thread

    // Serialize
    basis::core::MonotonicTime now = basis::core::MonotonicTime::Now();

    // Request size of payload from serializer
    const size_t payload_size = get_message_size_cb(*msg);
    // Create a packet of the proper size
    // TODO: embed time inside packet?
    auto packet = std::make_shared<MessagePacket>(MessageHeader::DataType::MESSAGE, payload_size);
    // Serialize directly to the packet
    std::span<std::byte> payload = packet->GetMutablePayload();
    if (!write_message_to_span_cb(*msg, payload)) {
      BASIS_LOG_ERROR("Unable to serialize message on topic {}", topic);
      return;
    }

    PublishRaw(std::move(packet), now);
  }

private:
  std::shared_ptr<InprocPublisher<T_MSG>> inproc;
  std::shared_ptr<InprocPublisher<T_CONVERTABLE_INPROC>> convertable_inproc;
  SerializeGetSizeCallback<T_MSG> get_message_size_cb;
  SerializeWriteSpanCallback<T_MSG> write_message_to_span_cb;
};

} // namespace basis::core::transport