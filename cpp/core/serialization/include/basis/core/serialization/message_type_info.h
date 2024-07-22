#pragma once

#include <string>

namespace basis::core::serialization {

struct MessageTypeInfo {
  std::string serializer;
  std::string name;

  std::string mcap_message_encoding;
  std::string mcap_schema_encoding;

  std::string SchemaId() const { return serializer + ":" + name; }
  // size_t type_size = 0; // Required for raw types, to help ensure safety
};

constexpr char MCAP_CHANNEL_METADATA_SERIALIZER[] = "basis_serializer";

} // namespace basis::core::serialization