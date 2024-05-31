cmake_minimum_required(VERSION 3.28)
project(basis)
enable_testing()

option(BASIS_ENABLE_ROS "Enable basis support for ROS" OFF)
if(BASIS_ENABLE_ROS)
    set(BASIS_ROS_ROOT /opt/ros/noetic CACHE STRING "ros root to use")
    add_compile_definitions(BASIS_ENABLE_ROS=1)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(cpp)
add_subdirectory(proto)