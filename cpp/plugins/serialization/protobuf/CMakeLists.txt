project(basis_plugins_serialization_protobuf)
include(FindProtobuf)
include(DeclarePlugin)
find_package(Protobuf REQUIRED)
find_package(absl REQUIRED COMPONENTS hash)

add_plugin(basis_plugins_serialization_protobuf src/protobuf.cpp)
target_include_directories(basis_plugins_serialization_protobuf PUBLIC include)
target_link_libraries(basis_plugins_serialization_protobuf
  basis::core::serialization
  basis::core::logging
  protobuf::libprotobuf
  absl::hash
  )

add_library(basis::plugins::serialization::protobuf ALIAS basis_plugins_serialization_protobuf)

if(${BASIS_ENABLE_TESTING})
  add_subdirectory(test)
endif()