#pragma once

#include <string>

namespace basis::core::serialization {

struct MessageTypeInfo {
  std::string serializer;
  std::string name;
  //size_t type_size = 0; // Required for raw types, to help ensure safety
};

} // namespace basis::core::transport