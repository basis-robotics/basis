project(basis_core_time)

add_library(basis_core_time STATIC src/time.cpp)
target_include_directories(basis_core_time PUBLIC include)

add_library(basis::core::time ALIAS basis_core_time)