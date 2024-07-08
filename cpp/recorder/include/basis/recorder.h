#pragma once
#define MCAP_COMPRESSION_NO_LZ4
#define MCAP_COMPRESSION_NO_ZSTD
#include <mcap/mcap.hpp>

#include <spdlog/spdlog.h>

#include <span>
#include <string_view>

#include <basis/core/time.h>

#include <filesystem>

namespace basis {
class Recorder {
public:
  Recorder(const std::filesystem::path &recording_dir = {}) : recording_dir(recording_dir) {}

  ~Recorder() { Stop(); }
  bool Start(std::string_view output_name) {
    std::string output_filename(output_name);
    output_filename += ".mcap";
     
    auto status = writer.open((recording_dir / output_filename).string(), mcap::McapWriterOptions("basis"));
    if (!status.ok()) {
      spdlog::error(status.message);
    }
    return status.ok();
  }

  void Stop() {}

  // TODO: take in a schema directly
  // TODO: write the additional crud about encodings into the schema
  // todo: we may want to store off schemas and topics for later
  bool RegisterTopic(std::string_view topic, std::string_view message_encoding, std::string_view schema_name,
                     std::string_view schema_encoding, std::string_view schema_data) {
    mcap::Schema schema(schema_name, schema_encoding, schema_data);
    writer.addSchema(schema);
    mcap::Channel channel(topic, message_encoding, schema.id);
    writer.addChannel(channel);
    topic_to_channel_id.emplace(topic, channel.id);
    return true;
  }

  bool WriteMessage(const std::string &topic, std::span<std::byte> payload, const basis::core::MonotonicTime &now) {
    mcap::Message msg;
    msg.channelId = topic_to_channel_id.at(topic);
    // msg.sequence = 1; // Optional
    msg.logTime = now.nsecs; // Required nanosecond timestamp
    msg.publishTime = msg.logTime; // for now
    msg.data = payload.data();
    msg.dataSize = payload.size();
    auto status = writer.write(msg);
    if (!status.ok()) {
      spdlog::error(status.message);
    }
    return status.ok();
  }

  mcap::McapWriter writer;

  std::unordered_map<std::string, mcap::SchemaId> topic_to_channel_id;
  std::filesystem::path recording_dir;
};
} // namespace basis