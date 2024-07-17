#include <basis/core/logging.h>


#include <spdlog/cfg/env.h>   // support for loading levels from the environment variable
#include "spdlog/fmt/ostr.h"  // support for user defined types

namespace basis::core::logging {

std::shared_ptr<LogHandler> global_logging_handler;
// Lock, mainly to prevent issues with shutting down while logging
// this may later go away - we may write the log handler in such a way as to allow for writing to disk all the way up to shutdown
// This will mainly require the recorder interface to be carried as a shared pointer
std::mutex global_logging_handler_mutex;

namespace internal {
  void RecorderSink::sink_it_(const spdlog::details::log_msg &msg) {
    // todo: if no global_logging_handler, buffer
    // if global_logging_handler, loop over previous items, write

    spdlog::memory_buf_t formatted;
    spdlog::sinks::base_sink<std::mutex>::formatter_->format(msg, formatted);
    // Take a reference here
    
    std::unique_lock lock(global_logging_handler_mutex);
    if(global_logging_handler) {
      // TODO the time here is slightly inaccurate, given the logging system is async
      // need to do math to convert the system clock embedded inside the message into a monotonic time
      global_logging_handler->HandleLog(basis::core::MonotonicTime::Now(), msg, fmt::to_string(formatted));
    }
  }
}

std::mutex create_log_mutex;

// TODO: we could instead enforce that the logger has the recorder pointer pushed in
// but then it would be fairly useless for system level functions
std::shared_ptr<spdlog::logger> CreateLogger(std::string&& logger_name) {
  std::unique_lock lock(create_log_mutex);
  auto logger = spdlog::get(logger_name);
  
  if(logger) {
    return logger;
  }
  logger = spdlog::create_async_nb<spdlog::sinks::stdout_color_sink_mt>(std::move(logger_name));

  logger->sinks().push_back(std::make_shared<internal::RecorderSink>());

  return logger;
}

void InitializeLoggingSystem() {
  // https://github.com/gabime/spdlog/blob/v1.x/include/spdlog/cfg/env.h
  spdlog::cfg::load_env_levels();
  // TODO - the time shown here is system over monotonic
  spdlog::set_pattern("[%E.%F] [%n] [%^%l%$] %v");
  // TODO: should we set a default logger here?
}

void SetLogHandler(std::shared_ptr<LogHandler> handler) {
  std::unique_lock lock(global_logging_handler_mutex);
  global_logging_handler = handler;
}


}