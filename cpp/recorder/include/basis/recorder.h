#pragma once
#define MCAP_COMPRESSION_NO_LZ4
#define MCAP_COMPRESSION_NO_ZSTD

#include <filesystem>
#include <regex>
#include <span>
#include <string_view>

#include <mcap/mcap.hpp>
#include <spdlog/spdlog.h>

#include <basis/core/containers/simple_mpsc.h>
#include <basis/core/serialization/message_type_info.h>
#include <basis/core/time.h>

namespace basis {

/**
 * Helper object to ensure that a span doesn't have a lifetime longer than a shared_ptr
 */
class OwningSpan {
public:
  template<typename T>
  OwningSpan(std::shared_ptr<T> ptr, const std::span<const std::byte>& span) 
    : owning_object(std::move(ptr)), span(span) {

  }

  const std::span<const std::byte>& Span() {
    return span;
  }

private:
  std::shared_ptr<const void> owning_object;
  std::span<const std::byte> span; 
};

/**
 * Basic interface
 *
 * Has two implemented variants:
 *  Recorder: used for immediate writes
 *  AsyncRecorder: used for background writes
 * 
 * Can be overriden in tests to have other behavior such as to check that a message was recorded or not
 */
class RecorderInterface {
public:
  // One might think to have this call "Stop()" - don't do that, the virtual function won't be dispatched properly
  virtual ~RecorderInterface() = default;

  virtual bool Start(std::string_view output_name) = 0;
  virtual void Stop() = 0;
  virtual bool RegisterTopic(const std::string& topic, const core::serialization::MessageTypeInfo& message_type_info, std::string_view schema_data) = 0;
  virtual bool WriteMessage(const std::string &topic, OwningSpan payload, const basis::core::MonotonicTime &now) = 0;
};

class Recorder : public RecorderInterface {
public:
  static const std::vector<std::regex> RECORD_ALL_TOPICS;

  Recorder(const std::filesystem::path &recording_dir = {}, const std::vector<std::regex>& topic_patterns = RECORD_ALL_TOPICS) : recording_dir(recording_dir), topic_patterns(topic_patterns) {

  }
  ~Recorder() { Stop(); }
  virtual bool Start(std::string_view output_name) override {
    std::string output_filename(output_name);
    output_filename += ".mcap";

    auto status = writer.open((recording_dir / output_filename).string(), mcap::McapWriterOptions("basis"));
    if (!status.ok()) {
      spdlog::error(status.message);
    }
    return status.ok();
  }

  virtual void Stop() override {
    writer.close();
  }

  // TODO: take in a schema directly
  // TODO: write the additional crud about encodings into the schema
  // todo: we may want to store off schemas and topics for later
  virtual bool RegisterTopic(const std::string& topic, const core::serialization::MessageTypeInfo& message_type_info, std::string_view schema_data) override {
    static const mcap::SchemaId invalid_schema_id = std::numeric_limits<mcap::SchemaId>::max();
    if(auto it = topic_to_channel_id.find(topic); it != topic_to_channel_id.end()) {
      return it->second != invalid_schema_id;
    }

    bool found_pattern = false;
    for(const std::regex& pattern : topic_patterns) {
      if(std::regex_match(topic, pattern)) {
        found_pattern = true;
        break;
      }
    }

    if(!found_pattern) {
      topic_to_channel_id.emplace(std::move(topic), invalid_schema_id);
      return false;
    }

    mcap::Schema schema(message_type_info.name, message_type_info.mcap_schema_encoding, schema_data);
    writer.addSchema(schema);
    mcap::Channel channel(topic, message_type_info.mcap_message_encoding, schema.id);
    writer.addChannel(channel);
    topic_to_channel_id.emplace(std::move(topic), channel.id);
    return true;
  }

  virtual bool WriteMessage(const std::string &topic, OwningSpan payload, const basis::core::MonotonicTime &now) override {
    return WriteMessage(topic, payload.Span(), now);
  }

  bool WriteMessage(const std::string &topic, const std::span<const std::byte>& payload, const basis::core::MonotonicTime &now) {
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

private:
  mcap::McapWriter writer;

  std::unordered_map<std::string, mcap::SchemaId> topic_to_channel_id;
  std::filesystem::path recording_dir;
  std::vector<std::regex> topic_patterns;
};

class AsyncRecorder : public RecorderInterface {
public:
  // TODO: max record queue size
  AsyncRecorder(const std::filesystem::path &recording_dir = {}, const std::vector<std::regex>& topic_patterns = Recorder::RECORD_ALL_TOPICS, bool drain_queue_on_stop = true) : drain_queue_on_stop(drain_queue_on_stop), recorder(recording_dir, topic_patterns) {
    
  }
  ~AsyncRecorder() { Stop(); }

  virtual bool Start(std::string_view output_name) {
    bool success = recorder.Start(output_name);
    if(success) {
      recording_thread = std::thread([this](){WorkThread();});
    }
    return success;
  }

  // TODO: it may be better to have a wait time and a force stop flag
  virtual void Stop() {
    stop = true;
    if(recording_thread.joinable()) {
      recording_thread.join();
    }

    recorder.Stop();
  }

  virtual bool RegisterTopic(const std::string& topic, const core::serialization::MessageTypeInfo& message_type_info, std::string_view schema_data) {
    std::lock_guard lock(mutex);
    return recorder.RegisterTopic(topic, message_type_info, schema_data);
  }

  virtual bool WriteMessage(const std::string &topic, OwningSpan payload, const basis::core::MonotonicTime &now) {
    work_queue.Emplace({topic, std::move(payload), now});
    return true;
  }

private:
  void WorkThread() {
    const basis::core::RealTimeDuration wait_time = basis::core::RealTimeDuration::FromSeconds(0.1);
    while(!stop) {
      auto event = work_queue.Pop(wait_time);
      if(event) {
        recorder.WriteMessage(event->topic, event->payload, event->stamp);
      }
    }

    if(drain_queue_on_stop) {
      while(auto event = work_queue.Pop()) {
        recorder.WriteMessage(event->topic, event->payload, event->stamp);
      }
    }
  }

  struct RecordEvent{
    std::string topic;
    OwningSpan payload;
    core::MonotonicTime stamp;
  };

  core::containers::SimpleMPSCQueue<RecordEvent> work_queue;

  bool drain_queue_on_stop;
  std::atomic<bool> stop = false;
  std::thread recording_thread;
  std::mutex mutex;
  Recorder recorder;
};
} // namespace basis