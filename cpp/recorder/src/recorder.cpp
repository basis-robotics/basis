#include "basis/core/serialization.h"
#include "basis/core/serialization/message_type_info.h"
#define MCAP_IMPLEMENTATION

#include <basis/recorder.h>

DECLARE_AUTO_LOGGER_NS(basis::recorder)

namespace basis::recorder {
const std::vector<std::regex> Recorder::RECORD_ALL_TOPICS = {std::regex(".*")};

bool Recorder::Split(std::string_view new_name) {
  Stop();

  return Start(new_name);
}

bool Recorder::WriteMessage(const std::string &topic, const std::span<const std::byte> &payload,
                            const basis::core::MonotonicTime &now) {
  mcap::Message msg;
  msg.channelId = topic_to_channel.at(topic)->id;
  // msg.sequence = 1; // Optional, though we should implement eventually
  msg.logTime = now.nsecs;       // Required nanosecond timestamp
  msg.publishTime = msg.logTime; // for now
  msg.data = payload.data();
  msg.dataSize = payload.size();
  auto status = writer.write(msg);
  if (!status.ok()) {
    BASIS_LOG_ERROR(status.message);
  }
  return status.ok();
}

bool Recorder::RegisterTopic(const std::string &topic, const core::serialization::MessageTypeInfo &message_type_info,
                             const core::serialization::MessageSchema& basis_schema) {
  if (auto it = topic_to_channel.find(topic); it != topic_to_channel.end()) {
    return it->second != std::nullopt;
  }

  bool found_pattern = false;
  for (const std::regex &pattern : topic_patterns) {
    if (std::regex_match(topic, pattern)) {
      found_pattern = true;
      break;
    }
  }

  if (!found_pattern) {
    topic_to_channel.emplace(std::move(topic), std::nullopt);
    return false;
  }

  auto schema_it = schema_id_to_mcap_schema.find(message_type_info.SchemaId());
  if (schema_it == schema_id_to_mcap_schema.end()) {
    mcap::Schema schema(message_type_info.name, message_type_info.mcap_schema_encoding, basis_schema.schema_efficient.empty() ? basis_schema.schema : basis_schema.schema_efficient);
    writer.addSchema(schema);
    schema_it = schema_id_to_mcap_schema.emplace(message_type_info.SchemaId(), std::move(schema)).first;
  }

  mcap::Channel channel(topic, message_type_info.mcap_message_encoding, schema_it->second.id);
  // Write the serializer as mcap's well known serializers are slightly different than basis's serializer name
  channel.metadata[core::serialization::MCAP_CHANNEL_METADATA_SERIALIZER] = message_type_info.serializer;
  // Write the hash ID and schema so that we don't have to load a serialization plugin up on replay to dump them
  channel.metadata[core::serialization::MCAP_CHANNEL_METADATA_HASH_ID] = basis_schema.hash_id;
  channel.metadata[core::serialization::MCAP_CHANNEL_METADATA_READABLE_SCHEMA] = basis_schema.schema;
  
  writer.addChannel(channel);
  topic_to_channel.emplace(std::move(topic), channel);
  return true;
}

} // namespace basis::recorder