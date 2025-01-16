add_executable(
  test_protobuf
  test_protobuf.cpp
)

find_package(absl REQUIRED COMPONENTS status)

target_link_libraries(
  test_protobuf
  GTest::gtest_main
  basis::plugins::serialization::protobuf basis_proto
  spdlog
  absl::status
)

include(GoogleTest REQUIRED)
gtest_discover_tests(test_protobuf)