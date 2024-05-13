#pragma once

#include <cassert>
#include <string_view>
#include <memory>
#include "inproc.h"
#include "message_type_info.h"
namespace basis {
namespace core {
namespace transport {
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

  virtual std::string GetPublisherInfo() = 0;
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

  virtual void Publish(std::shared_ptr<const T_MSG> msg) {
    assert(type_info.serializer == "raw");

    if(inproc) {
      inproc->Publish(msg);
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
  std::vector<std::shared_ptr<TransportPublisher>> transport_publishers;
};

} // namespace transport
} // namespace core
} // namespace basis