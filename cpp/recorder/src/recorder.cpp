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
                             std::string_view schema_data) {
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
    mcap::Schema schema(message_type_info.name, message_type_info.mcap_schema_encoding, schema_data);
    writer.addSchema(schema);
    schema_it = schema_id_to_mcap_schema.emplace(message_type_info.SchemaId(), std::move(schema)).first;
  }

  mcap::Channel channel(topic, message_type_info.mcap_message_encoding, schema_it->second.id);
  channel.metadata[core::serialization::MCAP_CHANNEL_METADATA_SERIALIZER] = message_type_info.serializer;
  writer.addChannel(channel);
  topic_to_channel.emplace(std::move(topic), channel);
  return true;
}

} // namespace basis::recorder