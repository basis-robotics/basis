project(basis_proto)
include(FindProtobuf)
find_package(Protobuf REQUIRED)

add_library(basis_proto SHARED 
  transport.proto
  time.proto
  test.proto
  basis_example.proto)
target_link_libraries(basis_proto basis::plugins::serialization::protobuf)

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

set(BASIS_GENERATED_DIR "${GENERATED_DIR}/basis_proto")
make_directory(${BASIS_GENERATED_DIR})
target_include_directories(basis_proto SYSTEM PUBLIC "$<BUILD_INTERFACE:${BASIS_GENERATED_DIR}>")

protobuf_generate(
  LANGUAGE cpp
  TARGET basis_proto 
  PROTOC_OUT_DIR "${BASIS_GENERATED_DIR}"
  )

# Import the foxglove protobuf files
FetchContent_Declare(foxglove_schemas
    GIT_REPOSITORY https://github.com/foxglove/schemas
    GIT_TAG 446d4f0c7450c0b9fb1c5b3889c2ea640ee045e6
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    )

FetchContent_MakeAvailable(foxglove_schemas)

set(FOXGLOVE_GENERATED_DIR "${GENERATED_DIR}")
make_directory(${FOXGLOVE_GENERATED_DIR})


set(FOXGLOVE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
# Need to copy in the foxglove protobufs to get protoc to be happy about paths - everything breaks if the protos aren't in a directory relative to the current one
file(COPY ${foxglove_schemas_SOURCE_DIR}/schemas/proto/foxglove DESTINATION ${FOXGLOVE_SOURCE_DIR})
file(GLOB FOXGLOVE_PROTOS ${FOXGLOVE_SOURCE_DIR}/foxglove/*.proto)

add_library(foxglove_schemas_protobuf SHARED ${FOXGLOVE_PROTOS})

protobuf_generate(
  LANGUAGE cpp
  TARGET foxglove_schemas_protobuf
  OUT_VAR FOOBAR
  PROTOC_OUT_DIR ${FOXGLOVE_GENERATED_DIR}
  IMPORT_DIRS ${FOXGLOVE_SOURCE_DIR}
  )

target_include_directories(foxglove_schemas_protobuf SYSTEM PUBLIC "$<BUILD_INTERFACE:${GENERATED_DIR}>")