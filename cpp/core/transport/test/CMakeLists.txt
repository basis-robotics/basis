
enable_testing()

add_executable(
  test_basic_transport
  test_basic_transport.cpp
)
target_link_libraries(
  test_basic_transport
  GTest::gtest_main
  basis::core::transport::inproc
)

include(GoogleTest REQUIRED)
gtest_discover_tests(test_basic_transport)
