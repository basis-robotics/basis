project(basis_proto)
include(FindProtobuf)
find_package(Protobuf REQUIRED)

add_library(basis_proto transport.proto test.proto)
target_link_libraries(basis_proto protobuf::libprotobuf)

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
make_directory(${GENERATED_DIR})
target_include_directories(basis_proto SYSTEM PUBLIC "$<BUILD_INTERFACE:${GENERATED_DIR}>")

protobuf_generate(
 LANGUAGE cpp
 TARGET basis_proto 
  PROTOC_OUT_DIR "${GENERATED_DIR}"
 )
