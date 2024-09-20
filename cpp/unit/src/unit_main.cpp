#include <spdlog/cfg/env.h>

#include <basis/unit.h>

extern "C" {
basis::Unit *CreateUnit();
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
  basis::core::logging::InitializeLoggingSystem();

  auto unit = std::unique_ptr<basis::Unit>(CreateUnit());
  unit->WaitForCoordinatorConnection();
  unit->CreateTransportManager();
  unit->Initialize();

  while (true) {
    unit->Update(nullptr, basis::core::Duration::FromSecondsNanoseconds(1, 0));
  }

  return 0;
}
