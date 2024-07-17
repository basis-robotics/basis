#include <basis/core/logging.h>

#include <foxglove/Log.pb.h>

// Helper to initialize the basis log with protobuf
// This can/will later be split into protobuf and ros1, with a compile time switch to configure the message type

namespace basis {

class ProtobufLogHandler : public core::logging::LogHandler {
public:
  ProtobufLogHandler(core::transport::TransportManager &transport_manager) {
    log_publisher = transport_manager.Advertise<foxglove::Log>("/log");
  }
  virtual ~ProtobufLogHandler() = default;

  virtual void HandleLog(const core::MonotonicTime &time, const spdlog::details::log_msg &log_msg,
                         std::string &&msg_formatted) override {
    auto proto_msg = std::make_shared<foxglove::Log>();

    const timespec ts = time.ToTimespec();

    proto_msg->mutable_timestamp()->set_seconds(ts.tv_sec);
    proto_msg->mutable_timestamp()->set_nanos(ts.tv_nsec);
    foxglove::Log::Level level = foxglove::Log::UNKNOWN;
    switch (log_msg.level) {
    case spdlog::level::level_enum::trace:
    case spdlog::level::level_enum::debug:
      level = foxglove::Log::DEBUG;
      break;
    case spdlog::level::level_enum::info:
      level = foxglove::Log::INFO;
      break;
    case spdlog::level::level_enum::warn:
      level = foxglove::Log::WARNING;
      break;
    case spdlog::level::level_enum::err:
      level = foxglove::Log::ERROR;
      break;
    case spdlog::level::level_enum::critical:
      level = foxglove::Log::FATAL;
      break;
    case spdlog::level::level_enum::off:
    case spdlog::level::level_enum::n_levels:
      break;
    }
    // level
    proto_msg->set_level(level);
    proto_msg->set_message(std::move(msg_formatted));
    proto_msg->set_name(std::string(log_msg.logger_name.data(), log_msg.logger_name.size()));
    if (log_msg.source.filename) {
      proto_msg->set_file(log_msg.source.filename);
    }
    proto_msg->set_line(log_msg.source.line);
    log_publisher->Publish(proto_msg);
  }

  std::shared_ptr<core::transport::Publisher<foxglove::Log>> log_publisher;
};

void CreateLogHandler(core::transport::TransportManager &transport_manager) {
  auto log_handler = std::make_shared<ProtobufLogHandler>(transport_manager);

  core::logging::SetLogHandler(std::move(log_handler));
}

void DestroyLogHandler() { core::logging::SetLogHandler(nullptr); }

} // namespace basis