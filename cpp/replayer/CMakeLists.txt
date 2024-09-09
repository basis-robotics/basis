add_library(basis_replayer SHARED src/replayer.cpp)

target_include_directories(basis_replayer PUBLIC include)
target_link_libraries(basis_replayer
  basis::core::coordinator
  basis::core::containers
  basis::core::logging
  basis::core::serialization
  basis::core::time
  basis::unit
  mcap)

add_library(basis::replayer ALIAS basis_replayer)

add_library(basis_replayer_args src/replay_args.cpp)
target_link_libraries(basis_replayer_args basis::replayer argparse)
add_library(basis::replayer::args ALIAS basis_replayer_args)


add_executable(replay src/replay.cpp)

target_link_libraries(replay basis::replayer basis::replayer::args)
# if(${BASIS_ENABLE_TESTING})
#   add_subdirectory(test)
# endif()