add_library(basis_core_containers INTERFACE)

target_include_directories(basis_core_containers INTERFACE include)

add_library(basis::core::containers ALIAS basis_core_containers)

target_link_libraries(
  basis_core_containers INTERFACE
  basis::core::time
)

if(${BASIS_ENABLE_TESTING})
  add_subdirectory(test)
endif()
