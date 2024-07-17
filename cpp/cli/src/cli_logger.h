#pragma once

#include <basis/core/logging/macros.h>
namespace basis::cli {

extern std::shared_ptr<spdlog::logger> basis_logger;
inline const std::shared_ptr<spdlog::logger>& AUTO_LOGGER(basis_logger);

}