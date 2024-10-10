add_library(basis_launch
  src/launch.cpp 
  src/launch_definition.cpp
  src/unit_loader.cpp
  src/process_manager.cpp)

target_include_directories(basis_launch PUBLIC include)

target_link_libraries(basis_launch 
  basis::unit
  basis::cli_logger
  Backward::Interface
  yaml-cpp
  dl
  )

add_library(basis::launch ALIAS basis_launch)

if(${BASIS_ENABLE_TESTING})
  add_subdirectory(test)
endif()
