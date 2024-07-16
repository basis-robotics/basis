project(basis_proto)
include(FindProtobuf)
find_package(Protobuf REQUIRED)

add_library(basis_proto SHARED transport.proto test.proto basis_example.proto)
target_link_libraries(basis_proto basis::plugins::serialization::protobuf)

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

set(BASIS_PROTO_DIR "${GENERATED_DIR}/basis_proto")
make_directory(${BASIS_PROTO_DIR})
target_include_directories(basis_proto SYSTEM PUBLIC "$<BUILD_INTERFACE:${BASIS_PROTO_DIR}>")

protobuf_generate(
  LANGUAGE cpp
  TARGET basis_proto 
  PROTOC_OUT_DIR "${BASIS_PROTO_DIR}"
  )

# Import the foxglove protobuf files
FetchContent_Declare(foxglove_schemas
    GIT_REPOSITORY https://github.com/foxglove/schemas
    GIT_TAG 446d4f0c7450c0b9fb1c5b3889c2ea640ee045e6
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    )

FetchContent_MakeAvailable(foxglove_schemas)

set(FOXGLOVE_PROTO_DIR "${GENERATED_DIR}")
make_directory(${FOXGLOVE_PROTO_DIR})

file(GLOB FOXGLOVE_PROTOS ${foxglove_schemas_SOURCE_DIR}/schemas/proto/foxglove/*.proto)

protobuf_generate(
  LANGUAGE cpp
  PROTOS ${FOXGLOVE_PROTOS}
  OUT_VAR FOXGLOVE_GENERATED_SOURCES
  PROTOC_OUT_DIR ${FOXGLOVE_PROTO_DIR}
  IMPORT_DIRS ${foxglove_schemas_SOURCE_DIR}/schemas/proto/
  )

# Strip the input tree structure from the output
list(TRANSFORM FOXGLOVE_GENERATED_SOURCES REPLACE "\\.\\./.*build/_deps/foxglove_schemas-src/schemas/proto/" "")

add_library(foxglove_schemas_protobuf SHARED ${FOXGLOVE_GENERATED_SOURCES})

target_include_directories(foxglove_schemas_protobuf SYSTEM PUBLIC "$<BUILD_INTERFACE:${FOXGLOVE_PROTO_DIR}>")