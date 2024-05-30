#pragma once

#include <string>
#include <unordered_map>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <transport.pb.h>
#pragma clang diagnostic pop

namespace basis::core::transport {

/**
 * Contains information needed to subscribe to a publisher
 * 
 * @todo: should we remove this and deal in raw proto? Would be faster.
 */
struct PublisherInfo {
  /**
   * Randomly generated, non colliding publisher id
   */
  union {
    __uint128_t publisher_id;
    uint64_t publisher_id_64s[2];
  };
  /**
   * The topic associated with the publisher
   */
  std::string topic;
  /**
   * The schema ID associated with the messages sent over the topic. 
   * ie "protobuf:Pose"
   */
  std::string schema_id;
  /**
   * Possible transports
   */
  std::unordered_map<std::string, std::string> transport_info;

  /**
   * Converts to a proto::PublisherInfo 
   */
  proto::PublisherInfo ToProto() {
    proto::PublisherInfo out;
    out.set_publisher_id_high(publisher_id_64s[0]);
    out.set_publisher_id_low(publisher_id_64s[1]);
    out.set_topic(topic);
    out.set_schema_id(schema_id);

    for(auto& p : transport_info) {
      out.mutable_transport_info()->insert({p.first, p.second});
    }
    return out;
  }
  
  /**
   * Converts from a proto::PublisherInfo 
   */
  static PublisherInfo FromProto(const proto::PublisherInfo& proto) {
    PublisherInfo out;
    out.publisher_id_64s[0] = proto.publisher_id_high();
    out.publisher_id_64s[1] = proto.publisher_id_low();
    out.topic = proto.topic();
    out.schema_id = proto.schema_id();
    for(auto& [topic, endpoint] : proto.transport_info()) {
      out.transport_info[topic] = endpoint;
    }
    return out;
  }
};

}