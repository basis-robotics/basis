project(simple_pub_sub_proto)
include(FindProtobuf)
find_package(Protobuf REQUIRED)

add_library(simple_pub_sub_proto SHARED simple_pub_sub.proto)
target_link_libraries(simple_pub_sub_proto basis::plugins::serialization::protobuf)
install(TARGETS simple_pub_sub_proto DESTINATION lib/)

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
make_directory(${GENERATED_DIR})
target_include_directories(simple_pub_sub_proto SYSTEM PUBLIC "$<BUILD_INTERFACE:${GENERATED_DIR}>")

protobuf_generate(
 LANGUAGE cpp
 TARGET simple_pub_sub_proto 
  PROTOC_OUT_DIR "${GENERATED_DIR}"
 )
