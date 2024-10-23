#include <basis/core/coordinator.h>
#include <basis/core/logging.h>

/**
 * Standalone Coordinator binary.
 */

using namespace basis::core::transport;
int main() {
  basis::core::logging::InitializeLoggingSystem();

  std::optional<Coordinator> coordinator = Coordinator::Create();
  if (!coordinator) {
    BASIS_LOG_ERROR_NS(coordinator, "Unable to create coordinator.");
    return 1;
  }
  auto next_sleep = std::chrono::steady_clock::now();
  while (true) {
    BASIS_LOG_TRACE_NS(coordinator, "Coordinator::Update()");
    next_sleep += std::chrono::milliseconds(50);
    coordinator->Update();
    std::this_thread::sleep_until(next_sleep);
  }
  return 0;
}