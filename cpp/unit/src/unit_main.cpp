#include <nonstd/expected.hpp>

#include <basis/unit.h>
#include <basis/unit/create_unit.h>

#include <basis/recorder/protobuf_log.h>

int main(int argc, char *argv[]) {
  basis::core::logging::InitializeLoggingSystem();

  std::unique_ptr<basis::Unit> unit(CreateUnit({}, std::pair{argc, argv}, [](const char* msg) {
    std::cerr << msg << std::endl;
  }));

  if(!unit) {
    return 1;
  }

  unit->WaitForCoordinatorConnection();
  auto* transport_manager = unit->CreateTransportManager();
  basis::CreateLogHandler(*transport_manager);
  
  unit->Initialize();

  while (true) {
    unit->Update(nullptr, basis::core::Duration::FromSecondsNanoseconds(1, 0));
  }

  return 0;
}
