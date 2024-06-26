add_executable(
  basis
  src/cli.cpp
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
