project(basis_plugins_serialization_rosmsg)

find_package(rostime REQUIRED PATHS /opt/ros/noetic/share/rostime/cmake)
find_package(roscpp_traits REQUIRED PATHS /opt/ros/noetic/share/roscpp_traits/cmake)
find_package(cpp_common REQUIRED PATHS /opt/ros/noetic/share/cpp_common/cmake)
find_package(roscpp_serialization REQUIRED PATHS /opt/ros/noetic/share/roscpp_serialization/cmake)
find_package(message_runtime REQUIRED PATHS /opt/ros/noetic/share/message_runtime/cmake)

find_package(std_msgs REQUIRED PATHS /opt/ros/noetic/share/std_msgs/cmake)

add_library(basis_plugins_serialization_rosmsg SHARED src/rosmsg.cpp)

add_library(ros_std_msgs INTERFACE)
target_include_directories(ros_std_msgs INTERFACE /opt/ros/noetic/include/)

target_include_directories(basis_plugins_serialization_rosmsg PUBLIC include )

target_link_directories(basis_plugins_serialization_rosmsg PUBLIC /opt/ros/noetic/lib)

target_link_libraries(basis_plugins_serialization_rosmsg basis_core_serialization roscpp_serialization ros_std_msgs)
# target_link_libraries(basis_plugins_serialization_rosmsg basis_proto)

add_library(basis::plugins::serialization::rosmsg ALIAS basis_plugins_serialization_rosmsg)

add_subdirectory(test)