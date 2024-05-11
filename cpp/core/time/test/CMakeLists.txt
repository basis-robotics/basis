enable_testing()

add_executable(
  test_time
  test_time.cpp
)
target_link_libraries(
  test_time
  GTest::gtest_main
  basis::core::transport::inproc
)

include(GoogleTest REQUIRED)
gtest_discover_tests(test_time)
