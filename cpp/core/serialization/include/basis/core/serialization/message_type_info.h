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

// In all likelihood, a schema should container a message type info
struct MessageSchema {
  std::string serializer;
  std::string name;
  std::string schema;
  std::string
      schema_efficient; // Optional - some serializers may have a more efficient representation wanted by the recorder
  std::string hash_id;
};

constexpr char MCAP_CHANNEL_METADATA_SERIALIZER[] = "basis_serializer";
constexpr char MCAP_CHANNEL_METADATA_READABLE_SCHEMA[] = "basis_human_readable_schema";
constexpr char MCAP_CHANNEL_METADATA_HASH_ID[] = "basis_hash_id";

} // namespace basis::core::serialization