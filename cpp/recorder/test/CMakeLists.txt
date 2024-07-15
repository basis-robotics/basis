include(GoogleTest REQUIRED)

add_executable(
  test_recorder
  test_recorder.cpp
)

target_link_libraries(
  test_recorder
  basis::recorder
  basis_proto
  GTest::gtest_main
  $<$<BOOL:${BASIS_ENABLE_ROS}>:basis::plugins::serialization::rosmsg>
)

gtest_discover_tests(test_recorder)