enable_testing()

add_library(libcoordinator SHARED src/coordinator.cpp)
target_include_directories(libcoordinator PUBLIC include)
target_link_libraries(
  libcoordinator
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
  libcoordinator
)
