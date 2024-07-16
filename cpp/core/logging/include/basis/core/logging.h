#pragma once


#include <basis/core/time.h>

#include <spdlog/sinks/base_sink.h>

#include "spdlog/sinks/stdout_color_sinks.h"

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <iostream>

namespace basis::core::logging {
namespace internal {

class RecorderSink : public spdlog::sinks::base_sink<std::mutex> {
protected:
  virtual void sink_it_(const spdlog::details::log_msg &msg) override;

  virtual void flush_() override {}

  // todo: buffer here, dump when callback set
};
} // namespace internal

class LogHandler {
public:
  virtual void HandleLog(const MonotonicTime& time, const spdlog::details::log_msg& log_msg, std::string&& formatted_message) = 0;
};


void InitializeLoggingSystem();

void SetLogHandler(std::shared_ptr<LogHandler> log_handler);

std::shared_ptr<spdlog::logger> CreateLogger(std::string logger_name);


} // namespace basis::core::logging