project(basis_core_networking)

add_library(basis_core_networking STATIC src/socket.cpp)
target_link_libraries(basis_core_networking )
target_include_directories(basis_core_networking PUBLIC include)

add_library(basis::core::networking ALIAS basis_core_networking)

add_subdirectory(test)
