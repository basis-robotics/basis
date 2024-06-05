project(basis_plugins_serialization_protobuf)
include(FindProtobuf)
find_package(Protobuf REQUIRED)

add_library(basis_plugins_serialization_protobuf SHARED src/protobuf.cpp)
target_include_directories(basis_plugins_serialization_protobuf PUBLIC include)
target_link_libraries(basis_plugins_serialization_protobuf basis_core_serialization spdlog)
# target_link_libraries(basis_plugins_serialization_protobuf basis_proto)

add_library(basis::plugins::serialization::protobuf ALIAS basis_plugins_serialization_protobuf)

if(${BASIS_ENABLE_TESTING})
  add_subdirectory(test)
endif()