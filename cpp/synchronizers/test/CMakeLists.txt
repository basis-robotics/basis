add_executable(
  test_synchronizers
  test_synchronizers.cpp
)
target_link_libraries(
  test_synchronizers
  GTest::gtest_main
  basis::synchronizers
)

include(GoogleTest REQUIRED)
gtest_discover_tests(test_synchronizers)
