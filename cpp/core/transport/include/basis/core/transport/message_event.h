#pragma once
#include <basis/core/time.h>
#include <memory>
#include <string>

namespace basis::core::transport {

struct TopicInfo {
  // TODO: a bunch of allocations here
  std::string topic;
  // std::string type;
  // std::string publisher_unit;
  // std::string publisher_host;
};

template <typename T_MSG> struct MessageEvent {
  MonotonicTime time;
  TopicInfo topic_info;

  std::shared_ptr<const T_MSG> message;
};

} // namespace basis::core::transport