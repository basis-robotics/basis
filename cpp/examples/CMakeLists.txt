
add_executable(
  basis_example
  src/basis_example.cpp
)
target_link_libraries(
  basis_example
  basis::plugins::transport::tcp
  basis::plugins::serialization::protobuf
  basis::core::coordinator
  argparse
)