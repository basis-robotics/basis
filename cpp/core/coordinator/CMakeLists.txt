add_library(basis_libcoordinator SHARED src/coordinator.cpp)
target_include_directories(basis_libcoordinator PUBLIC include)
target_link_libraries(
  basis_libcoordinator
  basis::plugins::transport::tcp
  basis::plugins::serialization::protobuf
  basis_proto
)

add_executable(
  coordinator
  src/coordinator_main.cpp
)
target_link_libraries(
  coordinator
  basis_libcoordinator
)

add_library(basis::core::coordinator ALIAS basis_libcoordinator)

if(${BASIS_ENABLE_TESTING})
  add_subdirectory(test)
endif()