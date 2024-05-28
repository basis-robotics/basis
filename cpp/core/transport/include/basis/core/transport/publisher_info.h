#pragma once

#include <string>
#include <unordered_map>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <transport.pb.h>
#pragma clang diagnostic pop

namespace basis::core::transport {

struct PublisherInfo {
  __uint128_t publisher_id;
  std::string topic;
  std::unordered_map<std::string, std::string> transport_info;

  proto::PublisherInfo ToProto() {
    proto::PublisherInfo out;
    out.set_publisher_id_high(publisher_id >> 8);
    out.set_publisher_id_low(publisher_id & 0xFFFFFFFF);
    out.set_topic(topic);
//    out.set_transport_info(transport_info);

    for(auto& p : transport_info) {
      // (*out.mutable_transport_info())[p.first] = p.second;
      out.mutable_transport_info()->insert({p.first, p.second});
    }
    return out;
  }
};

}