#include <basis/cli_logger.h>

namespace basis::cli {

std::shared_ptr<spdlog::logger> basis_logger = basis::core::logging::CreateLogger("basis");
// TODO: set up a special CLI logger that prints to stderr

}