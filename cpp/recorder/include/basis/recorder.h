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
#include <basis/core/logging/macros.h>
#include <basis/core/serialization/message_type_info.h>
#include <basis/core/time.h>

DEFINE_AUTO_LOGGER_NS(basis::recorder)

namespace basis {

/**
 * Helper object to ensure that a span doesn't have a lifetime longer than a shared_ptr
 */
class OwningSpan {
public:
  template <typename T>
  OwningSpan(std::shared_ptr<T> ptr, const std::span<const std::byte> &span)
      : owning_object(std::move(ptr)), span(span) {}

  const std::span<const std::byte> &Span() { return span; }

private:
  std::shared_ptr<const void> owning_object;
  std::span<const std::byte> span;
};

namespace recorder {

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
  virtual bool RegisterTopic(const std::string &topic, const core::serialization::MessageTypeInfo &message_type_info,
                             const core::serialization::MessageSchema& basis_schema) = 0;
  virtual bool WriteMessage(const std::string &topic, OwningSpan payload, const basis::core::MonotonicTime &now) = 0;
};

class Recorder : public RecorderInterface {
public:
  static const std::vector<std::regex> RECORD_ALL_TOPICS;

  Recorder(const std::filesystem::path &recording_dir = {},
           const std::vector<std::regex> &topic_patterns = RECORD_ALL_TOPICS)
      : recording_dir(recording_dir), topic_patterns(topic_patterns) {}
  ~Recorder() { Stop(); }

  virtual bool Start(std::string_view output_name) override {
    std::string output_filename(output_name);
    output_filename += ".mcap";

    auto status = writer.open((recording_dir / output_filename).string(), mcap::McapWriterOptions("basis"));
    if (!status.ok()) {
      BASIS_LOG_ERROR(status.message);
    }
    return status.ok();
  }

  virtual void Stop() override { writer.close(); }

  bool Split(std::string_view new_name);

  virtual bool RegisterTopic(const std::string &topic, const core::serialization::MessageTypeInfo &message_type_info,
                             const core::serialization::MessageSchema& basis_schema) override;

  virtual bool WriteMessage(const std::string &topic, OwningSpan payload,
                            const basis::core::MonotonicTime &now) override {
    return WriteMessage(topic, payload.Span(), now);
  }

  bool WriteMessage(const std::string &topic, const std::span<const std::byte> &payload,
                    const basis::core::MonotonicTime &now);

private:
  mcap::McapWriter writer;

  std::unordered_map<std::string, mcap::Schema> schema_id_to_mcap_schema;
  std::unordered_map<std::string, std::optional<mcap::Channel>> topic_to_channel;
  std::filesystem::path recording_dir;
  std::vector<std::regex> topic_patterns;
};

class AsyncRecorder : public RecorderInterface {
public:
  // TODO: max record queue size
  AsyncRecorder(const std::filesystem::path &recording_dir = {},
                const std::vector<std::regex> &topic_patterns = Recorder::RECORD_ALL_TOPICS,
                bool drain_queue_on_stop = true)
      : drain_queue_on_stop(drain_queue_on_stop), recorder(recording_dir, topic_patterns) {}
  ~AsyncRecorder() { Stop(); }

  virtual bool Start(std::string_view output_name) {
    bool success = recorder.Start(output_name);
    if (success) {
      recording_thread = std::thread([this]() { WorkThread(); });
    }
    return success;
  }

  // TODO: it may be better to have a wait time and a force stop flag
  virtual void Stop() {
    stop = true;
    if (recording_thread.joinable()) {
      recording_thread.join();
    }

    recorder.Stop();
  }

  virtual bool RegisterTopic(const std::string &topic, const core::serialization::MessageTypeInfo &message_type_info,
                             const core::serialization::MessageSchema& basis_schema) {
    std::lock_guard lock(mutex);
    return recorder.RegisterTopic(topic, message_type_info, basis_schema);
  }

  virtual bool WriteMessage(const std::string &topic, OwningSpan payload, const basis::core::MonotonicTime &now) {
    work_queue.Emplace({topic, std::move(payload), now});
    return true;
  }

private:
  void WorkThread() {
    const basis::core::RealTimeDuration wait_time = basis::core::RealTimeDuration::FromSeconds(0.1);
    while (!stop) {
      auto event = work_queue.Pop(wait_time);
      if (event) {
        recorder.WriteMessage(event->topic, event->payload, event->stamp);
      }
    }

    if (drain_queue_on_stop) {
      while (auto event = work_queue.Pop()) {
        recorder.WriteMessage(event->topic, event->payload, event->stamp);
      }
    }
  }

  struct RecordEvent {
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
} // namespace recorder

using recorder::AsyncRecorder;
using recorder::Recorder;
using recorder::RecorderInterface;

} // namespace basis