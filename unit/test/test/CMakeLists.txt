
add_executable(
  test_unit_generation
  test_unit_generation.cpp
)

target_link_libraries(
  test_unit_generation
  unit::test_unit
  GTest::gtest_main
)

include(GoogleTest REQUIRED)
gtest_discover_tests(test_unit_generation)