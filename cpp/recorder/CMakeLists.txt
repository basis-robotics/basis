add_library(basis_recorder SHARED src/recorder.cpp)

target_include_directories(basis_recorder PUBLIC include)
target_link_libraries(basis_recorder
  basis::core::containers
  basis::core::logging
  basis::core::serialization
  basis::core::time
  foxglove_schemas_protobuf
  mcap)
add_library(basis::recorder ALIAS basis_recorder)

if(${BASIS_ENABLE_TESTING})
  add_subdirectory(test)
endif()