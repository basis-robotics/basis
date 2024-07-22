#include "basis/core/logging/macros.h"
#include "basis/core/time.h"
#include "basis/core/transport/publisher.h"
#include "mcap/errors.hpp"
#include "mcap/types.hpp"
#include <basis/replayer.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

#include <basis/unit.h>

DECLARE_AUTO_LOGGER_NS(basis::replayer)


namespace basis {
using namespace replayer;
void LogMcapFailure(mcap::Status status) {
  BASIS_LOG_ERROR("Mcap failure: {}", status.message);
}

bool Replayer::LoadRecording(std::filesystem::path recording_path) {
  auto status = mcap_reader.open(std::string(recording_path));
  status = mcap_reader.readSummary(mcap::ReadSummaryMethod::AllowFallbackScan);

  for (const auto &[channel_id, channel] : mcap_reader.channels()) {
    [[maybe_unused]] const std::string &topic = channel->topic;
    core::serialization::MessageTypeInfo message_type;
    std::shared_ptr<mcap::Schema> schema = mcap_reader.schema(channel->schemaId);
    message_type.name = schema->name;
    message_type.serializer = channel->metadata[core::serialization::MCAP_CHANNEL_METADATA_SERIALIZER];

    message_type.mcap_message_encoding = channel->messageEncoding;

    message_type.mcap_schema_encoding = schema->encoding;

    std::shared_ptr<core::transport::PublisherRaw> publisher =
        transport_manager.AdvertiseRaw(topic, message_type, {(char *)schema->data.data(), schema->data.size()});

    if (!publisher) {
      BASIS_LOG_ERROR("Failed to create raw publisher on {}", topic);

      return false;
    }

    publishers.emplace(topic, publisher);

    BASIS_LOG_INFO("replaying topic {}", topic);
  }

  return true;
}

bool Replayer::Run() {
  bool ok = LoadRecording(config.input);

  if (!ok) {
    return false;
  }


  const auto &statistics = mcap_reader.statistics();
  basis::core::MonotonicTime now;
  now.nsecs = (int64_t)statistics->messageStartTime;


  mcap::ReadMessageOptions options;
  options.readOrder = mcap::ReadMessageOptions::ReadOrder::LogTimeOrder;

  auto message_view = mcap_reader.readMessages(LogMcapFailure, options);

  basis::StandardUpdate(&transport_manager, &coordinator_connector);

  for(const mcap::MessageView& message : message_view) {
    while(now.nsecs < (int64_t)message.message.publishTime) {
      constexpr int64_t NSECS_PER_TICK = 1000000; // 1ms
      now.nsecs += NSECS_PER_TICK;
      std::this_thread::sleep_for(std::chrono::nanoseconds(NSECS_PER_TICK));
      basis::StandardUpdate(&transport_manager, &coordinator_connector);

    }
    BASIS_LOG_INFO("Publishing on {}", message.channel->topic);

    auto packet = std::make_shared<core::transport::MessagePacket>(core::transport::MessageHeader::DataType::MESSAGE, message.message.dataSize);
    memcpy(packet->GetMutablePayload().data(), message.message.data, message.message.dataSize);
    publishers.at(message.channel->topic)->PublishRaw(packet, now);
  }

  return ok;
}

} // namespace basis