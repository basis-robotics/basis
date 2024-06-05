project(basis_plugins_serialization_rosmsg)

# Manually find required ros libraries
# This should be moved out to a helper library if/when ros connections are support
# Ideally we stay far away from catkin here
# Two options for cleaning this up:
#   1. Attempt to figure out how to use find_package with ROS - probably via CMAKE_PREFIX_PATH
#   2. fork and modify https://github.com/ros/roscpp_core/blob/noetic-devel/doc/roscpp.rst for a smaller footprint

message("using BASIS_ROS_ROOT=${BASIS_ROS_ROOT}")

add_plugin(basis_plugins_serialization_rosmsg src/rosmsg.cpp)

target_include_directories(basis_plugins_serialization_rosmsg PUBLIC include)
target_include_directories(basis_plugins_serialization_rosmsg PUBLIC ${BASIS_ROS_ROOT}/include/)

target_link_directories(basis_plugins_serialization_rosmsg PUBLIC ${BASIS_ROS_ROOT}/lib)

target_link_libraries(basis_plugins_serialization_rosmsg PUBLIC basis_core_serialization roscpp_serialization rosx_introspection)

add_library(basis::plugins::serialization::rosmsg ALIAS basis_plugins_serialization_rosmsg)

if(${BASIS_ENABLE_TESTING})
  add_subdirectory(test)
endif()