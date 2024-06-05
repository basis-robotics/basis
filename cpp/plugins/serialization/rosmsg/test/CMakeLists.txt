find_package(geometry_msgs REQUIRED PATHS ${BASIS_ROS_ROOT}/share/geometry_msgs/cmake)
find_package(sensor_msgs REQUIRED PATHS ${BASIS_ROS_ROOT}/share/sensor_msgs/cmake)

add_executable(
  test_rosmsg
  test_rosmsg.cpp
)

target_link_libraries(
  test_rosmsg
  GTest::gtest_main
  basis::plugins::serialization::rosmsg spdlog
)

include(GoogleTest REQUIRED)
gtest_discover_tests(test_rosmsg)