
add_executable(
  basis_example
  src/basis_example.cpp
)
target_link_libraries(
  basis_example
  basis::unit
  basis::plugins::transport::tcp
  basis::plugins::serialization::protobuf
  basis::core::coordinator
  basis::synchronizers
  argparse
)

if(BASIS_ENABLE_ROS)

find_package(std_msgs REQUIRED PATHS ${BASIS_ROS_ROOT}/share/std_msgs/cmake)
find_package(geometry_msgs REQUIRED PATHS ${BASIS_ROS_ROOT}/share/geometry_msgs/cmake)
find_package(sensor_msgs REQUIRED PATHS ${BASIS_ROS_ROOT}/share/sensor_msgs/cmake)

target_link_libraries(
  basis_example
  rostime
  basis::plugins::serialization::rosmsg)

endif()