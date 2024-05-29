add_executable(
  basis
  src/cli.cpp
)

target_link_libraries(
  basis
  basis::plugins::transport::tcp
  basis::plugins::serialization::protobuf
  basis::core::coordinator
  argparse
)