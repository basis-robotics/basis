#include <basis/core/coordinator.h>
#include <spdlog/cfg/env.h>

/**
 * Standalone Coordinator binary.
 */

using namespace basis::core::transport;
int main() {
  spdlog::cfg::load_env_levels();

  std::optional<Coordinator> coordinator = Coordinator::Create();
  if (!coordinator) {
    spdlog::error("Unable to create coordinator.");
    return 1;
  }
  auto next_sleep = std::chrono::steady_clock::now();
  while (true) {
    spdlog::trace("Coordinator::Update()");
    next_sleep += std::chrono::milliseconds(50);
    coordinator->Update();
    std::this_thread::sleep_until(next_sleep);
  }
  return 0;
}