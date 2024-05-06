project(basis_core_transport)

add_library(basis_core_transport_inproc STATIC src/inproc.cpp)
target_link_libraries(basis_core_transport_inproc basis::core::time)
target_include_directories(basis_core_transport_inproc PUBLIC include)

add_library(basis::core::transport::inproc ALIAS basis_core_transport_inproc)
add_library(basis::core::transport ALIAS basis_core_transport_inproc)

add_subdirectory(test)
