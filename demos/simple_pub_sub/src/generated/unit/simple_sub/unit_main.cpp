#include <simple_sub.h>

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
  //spdlog::cfg::load_env_levels();

  simple_sub unit;
  unit.WaitForCoordinatorConnection();
  unit.CreateTransportManager();
  unit.Initialize();

  while (true) {
    unit.Update(1);
  }

  return 0;
}