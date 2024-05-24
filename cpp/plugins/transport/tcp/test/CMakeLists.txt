enable_testing()

add_executable(
  test_tcp_transport
  test_tcp_transport.cpp
)
target_link_libraries(
  test_tcp_transport
  GTest::gtest_main
  basis::plugins::transport::tcp
  basis::plugins::serialization::protobuf
  basis_proto
)

include(GoogleTest REQUIRED)
gtest_discover_tests(test_tcp_transport)
