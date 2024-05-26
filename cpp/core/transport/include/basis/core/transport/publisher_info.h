#include <string>
#include <unordered_map>

namespace basis::core::transport {

struct PublisherInfo {
  __uint128_t publisher_id;
  std::string topic;
  std::unordered_map<std::string, std::string> transport_info;
};

}