#include <simple_pub.h>

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
  //spdlog::cfg::load_env_levels();

  simple_pub unit;
  unit.WaitForCoordinatorConnection();
  unit.CreateTransportManager();
  unit.Initialize();

  while (true) {
    unit.Update(1);
  }

  return 0;
}