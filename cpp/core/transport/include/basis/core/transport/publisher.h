#pragma once

#include <cassert>
#include <string_view>
#include <memory>
#include "inproc.h"
#include "message_type_info.h"
#include "message_packet.h"
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
Publisher(std::string_view topic, MessageTypeInfo type_info, std::vector<std::shared_ptr<TransportPublisher>> transport_publishers, std::shared_ptr<InprocPublisher<T_MSG>> inproc)
      : topic(topic), type_info(type_info), inproc(inproc), transport_publishers(transport_publishers) {}
  
  std::vector<std::string> GetPublisherInfo() {
    std::vector<std::string> out;
    for(auto& pub : transport_publishers) {
      out.push_back(pub->GetPublisherInfo());
    }
    return out;
  }

  size_t GetSubscriberCount() {
    size_t n = 0;
      for(auto& transport_publisher : transport_publishers) {
        n+= transport_publisher->GetSubscriberCount();
      }
      return n;
  }


  virtual void Publish(std::shared_ptr<const T_MSG> msg) {
    assert(type_info.serializer == "raw");
  
    if(inproc) {
      inproc->Publish(msg);
    }

    // TODO: early out if no transports in any publisher

    // temporary raw only serialization
    auto packet = std::make_shared<MessagePacket>(MessageHeader::DataType::MESSAGE, sizeof(T_MSG));
    std::span<std::byte> payload = packet->GetMutablePayload();
    memcpy(payload.data(), msg.get(), sizeof(T_MSG));

    for(auto& pub : transport_publishers) {
      pub->SendMessage(packet);
    }


/*
    1. serialize
    2. publish


    for(auto& pub : transport_publishers) {
      pub->Publish(msg);
    }
    */
  }

  const std::string topic;
  const MessageTypeInfo type_info;
  std::shared_ptr<InprocPublisher<T_MSG>> inproc;
  // TODO: these are shared_ptrs - it could be a single unique_ptr if we were sure we never want to pool these
  std::vector<std::shared_ptr<TransportPublisher>> transport_publishers;
};

}