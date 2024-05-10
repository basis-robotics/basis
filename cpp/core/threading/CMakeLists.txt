project(basis_core_threading)

add_library(basis_core_threading INTERFACE)
target_include_directories(basis_core_threading INTERFACE include)

add_library(basis::core::threading ALIAS basis_core_threading)
