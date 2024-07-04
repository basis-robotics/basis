add_executable(
  basis
  src/cli.cpp
  src/launch.cpp
  src/unit_loader.cpp
)

target_link_libraries(
  basis
  basis::plugins::transport::tcp
  basis::core::coordinator
  basis::unit
  argparse
  dl
  yaml-cpp
)
