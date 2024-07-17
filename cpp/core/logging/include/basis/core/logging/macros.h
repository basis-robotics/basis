#pragma once

#include <basis/core/logging.h>

#include <spdlog/spdlog.h>

// Convenience macros to do "the right thing" given an appropriately scoped logger
#define BASIS_LOG_TRACE(...) SPDLOG_LOGGER_TRACE(AUTO_LOGGER, __VA_ARGS__)
#define BASIS_LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(AUTO_LOGGER, __VA_ARGS__)
#define BASIS_LOG_INFO(...) SPDLOG_LOGGER_INFO(AUTO_LOGGER, __VA_ARGS__)
#define BASIS_LOG_WARN(...) SPDLOG_LOGGER_WARN(AUTO_LOGGER, __VA_ARGS__)
#define BASIS_LOG_ERROR(...) SPDLOG_LOGGER_ERROR(AUTO_LOGGER, __VA_ARGS__)
#define BASIS_LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(AUTO_LOGGER, __VA_ARGS__)
#define BASIS_LOG_FATAL(...) SPDLOG_LOGGER_CRITICAL(AUTO_LOGGER, __VA_ARGS__)



#define DEFINE_AUTO_LOGGER_NS(NAMESPACE) \
namespace NAMESPACE { \
  extern std::shared_ptr<spdlog::logger> global_logger; \
  inline const std::shared_ptr<spdlog::logger>& AUTO_LOGGER(global_logger); \
}

namespace basis::core::logging {
  consteval std::string_view StripLeadingNamespace(std::string_view ns) {
    return ns;
  }
}

#define DECLARE_AUTO_LOGGER_NS(NAMESPACE) \
namespace NAMESPACE { \
  std::shared_ptr<spdlog::logger> global_logger = basis::core::logging::CreateLogger( \
    std::string(basis::core::logging::StripLeadingNamespace(#NAMESPACE)) \
  ); \
}

//#define ASSERT(...)