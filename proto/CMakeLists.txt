project(basis_proto)
include(FindProtobuf)
find_package(Protobuf REQUIRED)

add_library(basis_proto test.cpp test.proto)
target_link_libraries(basis_proto protobuf::libprotobuf)

protobuf_generate(
 LANGUAGE cpp
 TARGET basis_proto 
 )