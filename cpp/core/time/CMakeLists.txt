project(basis_core_time)

add_library(basis_core_time STATIC src/time.cpp)
target_include_directories(basis_core_time PUBLIC include)

#removeme
target_link_libraries(basis_core_time spdlog)

add_library(basis::core::time ALIAS basis_core_time)

add_subdirectory(test)