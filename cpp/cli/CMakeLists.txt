add_executable(
  basis
  src/cli.cpp
  src/launch.cpp
  src/unit_loader.cpp
  src/process_manager.cpp
)

target_link_libraries(
  basis
  basis::plugins::transport::tcp
  basis::core::coordinator
  basis::unit
  argparse
  Backward::Interface
  dl
  yaml-cpp
)
