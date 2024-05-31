project(basis_plugins_serialization_rosmsg)

# Manually find required ros libraries
# This should be moved out to a helper library if/when ros connections are support
# Ideally we stay far away from catkin here
# Two options for cleaning this up:
#   1. Attempt to figure out how to use find_package with ROS - probably via CMAKE_PREFIX_PATH
#   2. fork and modify https://github.com/ros/roscpp_core/blob/noetic-devel/doc/roscpp.rst for a smaller footprint

message("using BASIS_ROS_ROOT=${BASIS_ROS_ROOT}")

find_package(cpp_common REQUIRED PATHS ${BASIS_ROS_ROOT}/share/cpp_common/cmake)
find_package(rostime REQUIRED PATHS ${BASIS_ROS_ROOT}/share/rostime/cmake)
find_package(roscpp_traits REQUIRED PATHS ${BASIS_ROS_ROOT}/share/roscpp_traits/cmake)
find_package(roscpp_serialization REQUIRED PATHS ${BASIS_ROS_ROOT}/share/roscpp_serialization/cmake)
find_package(message_runtime REQUIRED PATHS ${BASIS_ROS_ROOT}/share/message_runtime/cmake)

# TODO: maybe not strictly required
# find_package(std_msgs REQUIRED PATHS ${BASIS_ROS_ROOT}/share/std_msgs/cmake)

add_library(basis_plugins_serialization_rosmsg SHARED src/rosmsg.cpp)

add_library(ros_std_msgs INTERFACE)
target_include_directories(ros_std_msgs INTERFACE ${BASIS_ROS_ROOT}/include/)

target_include_directories(basis_plugins_serialization_rosmsg PUBLIC include)

target_link_directories(basis_plugins_serialization_rosmsg PUBLIC ${BASIS_ROS_ROOT}/lib)

target_link_libraries(basis_plugins_serialization_rosmsg PUBLIC basis_core_serialization roscpp_serialization ros_std_msgs rosx_introspection)

add_library(basis::plugins::serialization::rosmsg ALIAS basis_plugins_serialization_rosmsg)

add_subdirectory(test)