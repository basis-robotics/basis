cmake_minimum_required(VERSION 3.28)

project(SimplePubSub)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(/basis/ /basis/build)
add_subdirectory(proto)

generate_unit(simple_pub)
generate_unit(simple_sub)

target_link_libraries(unit_simple_pub simple_pub_sub_proto)
target_link_libraries(unit_simple_sub simple_pub_sub_proto)
