add_library(basis_core_containers INTERFACE)

target_include_directories(basis_core_containers INTERFACE include)

add_library(basis::core::containers ALIAS basis_core_containers)