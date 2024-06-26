cmake_minimum_required(VERSION 3.28)

project(SimplePubSub)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(/basis/ /basis/build)
add_subdirectory(proto)

# add_executable(
#   SimplePubSub 
#   src/simple_pub_sub.cpp 
#   src/generated/unit/simple_sub/unit_base.cpp 
#   src/generated/unit/simple_pub/unit_base.cpp
# )

# set(BASIS_SOURCE_ROOT /basis)

generate_unit(simple_pub)
generate_unit(simple_sub)

target_link_libraries(unit_simple_pub simple_pub_sub_proto)
target_link_libraries(unit_simple_sub simple_pub_sub_proto)

# target_link_libraries(
#   SimplePubSub
#   basis::unit
#   basis::plugins::transport::tcp
#   basis::plugins::serialization::protobuf
#   basis::core::coordinator
#   basis::synchronizers
#   simple_pub_sub_proto
#   # argparse
# )
