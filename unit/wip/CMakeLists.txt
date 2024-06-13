include(Unit)

find_package(sensor_msgs REQUIRED PATHS ${BASIS_ROS_ROOT}/share/sensor_msgs/cmake)

generate_unit(wip)

# TODO: query the yaml for the types
target_link_libraries(wip basis::plugins::serialization::rosmsg rostime) 