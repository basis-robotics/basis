#pragma once
#define MCAP_COMPRESSION_NO_LZ4
#define MCAP_COMPRESSION_NO_ZSTD
#include <mcap/mcap.hpp>

#include <spdlog/spdlog.h>

#include <span>
#include <string_view>

#include <basis/core/time.h>

#include <filesystem>

#include <basis/core/containers/simple_mpsc.h>

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

// template<typename T>
// OwningSpan CreateOwningSpan(std::shared_ptr<T> ptr, const std::span<const std::byte>& span) {
//   return OwningSpan(std::move(ptr), span);
// }

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

  // TODO: SetFilter

  virtual bool Start(std::string_view output_name) = 0;
  virtual void Stop() = 0;
  virtual bool RegisterTopic(std::string_view topic, std::string_view message_encoding, std::string_view schema_name,
                     std::string_view schema_encoding, std::string_view schema_data) = 0;
  virtual bool WriteMessage(const std::string &topic, OwningSpan payload, const basis::core::MonotonicTime &now) = 0;
};

class Recorder : public RecorderInterface {
public:
  Recorder(const std::filesystem::path &recording_dir = {}) : recording_dir(recording_dir) {

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

  virtual void Stop() override {}

  // TODO: take in a schema directly
  // TODO: write the additional crud about encodings into the schema
  // todo: we may want to store off schemas and topics for later
  virtual bool RegisterTopic(std::string_view topic, std::string_view message_encoding, std::string_view schema_name,
                     std::string_view schema_encoding, std::string_view schema_data) override {
    mcap::Schema schema(schema_name, schema_encoding, schema_data);
    writer.addSchema(schema);
    mcap::Channel channel(topic, message_encoding, schema.id);
    writer.addChannel(channel);
    topic_to_channel_id.emplace(topic, channel.id);
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
};

class AsyncRecorder : public RecorderInterface {
public:
  // TODO: max record queue size
  AsyncRecorder( const std::filesystem::path &recording_dir = {}, bool drain_queue_on_stop = true) : drain_queue_on_stop(drain_queue_on_stop), recorder(recording_dir) {
    
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
  }

  virtual bool RegisterTopic(std::string_view topic, std::string_view message_encoding, std::string_view schema_name,
                     std::string_view schema_encoding, std::string_view schema_data) {
    std::lock_guard lock(mutex);
    return recorder.RegisterTopic(topic, message_encoding, schema_name, schema_encoding, schema_data);
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
        spdlog::info("event");
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