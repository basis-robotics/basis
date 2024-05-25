enable_testing()

add_executable(
  test_protobuf
  test_protobuf.cpp
)
target_link_libraries(
  test_protobuf
  GTest::gtest_main
  basis::plugins::serialization::protobuf basis_proto
)

include(GoogleTest REQUIRED)
gtest_discover_tests(test_protobuf)