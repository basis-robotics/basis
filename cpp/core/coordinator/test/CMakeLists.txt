find_package(absl REQUIRED)
add_executable(
  test_coordinator
  test_coordinator.cpp
)
target_link_libraries(
  test_coordinator
  GTest::gtest_main
  basis::core::coordinator
  absl::hash
)

include(GoogleTest REQUIRED)
gtest_discover_tests(test_coordinator)
