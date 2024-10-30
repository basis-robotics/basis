add_executable(
  test_containers
  test_containers.cpp
)
target_link_libraries(
  test_containers
  GTest::gtest_main
)

target_link_libraries(
    test_containers
    basis::core::containers
)

include(GoogleTest REQUIRED)
gtest_discover_tests(test_containers)
