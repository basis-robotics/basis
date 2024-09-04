#include <basis/core/logging/macros.h>
#include <basis/core/time.h>
#include <basis/core/transport/publisher.h>
#include <basis/replayer.h>
#include <basis/unit.h>
#include <chrono>
#include <cstdint>
#include <google/protobuf/wrappers.pb.h>
#include <mcap/errors.hpp>
#include <mcap/types.hpp>
#include <memory>
#include <thread>

DECLARE_AUTO_LOGGER_NS(basis::replayer)

namespace basis {
using namespace replayer;
void LogMcapFailure(mcap::Status status) { BASIS_LOG_ERROR("Mcap failure: {}", status.message); }

bool Replayer::LoadRecording(std::filesystem::path recording_path) {
  auto status = mcap_reader.open(std::string(recording_path));
  status = mcap_reader.readSummary(mcap::ReadSummaryMethod::AllowFallbackScan);

  if(!status.ok()) {
    return false;
  }

  return OnRecordLoaded();
}

bool Replayer::OnRecordLoaded() {
  for (const auto &[channel_id, channel] : mcap_reader.channels()) {
    [[maybe_unused]] const std::string &topic = channel->topic;
    core::serialization::MessageTypeInfo message_type;
    std::shared_ptr<mcap::Schema> mcap_schema = mcap_reader.schema(channel->schemaId);

    message_type.name = mcap_schema->name;
    message_type.serializer = channel->metadata[core::serialization::MCAP_CHANNEL_METADATA_SERIALIZER];
    message_type.mcap_message_encoding = channel->messageEncoding;
    message_type.mcap_schema_encoding = mcap_schema->encoding;

    core::serialization::MessageSchema basis_schema;
    basis_schema.name = message_type.name;
    basis_schema.serializer = message_type.serializer;
    basis_schema.schema = channel->metadata[core::serialization::MCAP_CHANNEL_METADATA_READABLE_SCHEMA];
    basis_schema.hash_id = channel->metadata[core::serialization::MCAP_CHANNEL_METADATA_HASH_ID];
    basis_schema.schema_efficient = std::string((const char *)mcap_schema->data.data(), mcap_schema->data.size());

    std::shared_ptr<core::transport::PublisherRaw> publisher =
        transport_manager.AdvertiseRaw(topic, message_type, basis_schema);

    if (!publisher) {
      BASIS_LOG_ERROR("Failed to create raw publisher on {}", topic);

      return false;
    }

    publishers.emplace(topic, publisher);

    time_publisher = transport_manager.Advertise<basis::core::transport::proto::Time>("/time");

    BASIS_LOG_INFO("replaying topic {}", topic);
  }

  return true;
}

bool Replayer::Run() {
  bool ok = LoadRecording(config.input);

  if (!ok) {
    return false;
  }

  do {
    const auto &statistics = mcap_reader.statistics();
    basis::core::MonotonicTime now;
    now.nsecs = (int64_t)statistics->messageStartTime;
    BASIS_LOG_INFO("Beginning replay at {}", now.ToSeconds());
    mcap::ReadMessageOptions options;
    options.readOrder = mcap::ReadMessageOptions::ReadOrder::LogTimeOrder;

    auto message_view = mcap_reader.readMessages(LogMcapFailure, options);

    basis::StandardUpdate(&transport_manager, &coordinator_connector);

    const int64_t token = basis::core::MonotonicTime::Now().nsecs;

    auto wall_next = std::chrono::steady_clock::now();
    for (const mcap::MessageView &message : message_view) {
      while (now.nsecs < (int64_t)message.message.publishTime) {
        constexpr int64_t NSECS_PER_TICK = 10000000; // 100hz
        now.nsecs += NSECS_PER_TICK;
        wall_next += std::chrono::nanoseconds(NSECS_PER_TICK);
        std::this_thread::sleep_until(wall_next);
        basis::StandardUpdate(&transport_manager, &coordinator_connector);
        auto time_message = std::make_shared<basis::core::transport::proto::Time>();
        time_message->set_nsecs(now.nsecs);
        time_message->set_run_token(*reinterpret_cast<const uint64_t*>(&token));
        time_publisher->Publish(time_message);
      }
      BASIS_LOG_DEBUG("Publishing on {}", message.channel->topic);

      auto packet = std::make_shared<core::transport::MessagePacket>(core::transport::MessageHeader::DataType::MESSAGE,
                                                                     message.message.dataSize);
      memcpy(packet->GetMutablePayload().data(), message.message.data, message.message.dataSize);
      publishers.at(message.channel->topic)->PublishRaw(packet, now);
    }
  } while (config.loop);

  return ok;
}

} // namespace basis