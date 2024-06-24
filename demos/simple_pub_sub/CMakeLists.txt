cmake_minimum_required(VERSION 3.28)

project(SimplePubSub)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(/basis/ /basis/build)
add_subdirectory(proto)

add_executable(
  SimplePubSub 
  src/simple_pub_sub.cpp 
  src/generated/unit/simple_sub/unit_base.cpp 
  src/generated/unit/simple_pub/unit_base.cpp
)

# generate_unit(simple_pub)

target_link_libraries(
  SimplePubSub
  basis::unit
  basis::plugins::transport::tcp
  basis::plugins::serialization::protobuf
  basis::core::coordinator
  basis::synchronizers
  simple_pub_sub_proto
#   argparse
)
