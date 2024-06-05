add_executable(
  test_tcp
  test_tcp.cpp
)
target_link_libraries(
  test_tcp
  GTest::gtest_main
  basis::core::networking
)

include(GoogleTest REQUIRED)
gtest_discover_tests(test_tcp)
