project(basis_plugins_transport_tcp)

add_library(basis_plugins_transport_tcp SHARED src/tcp.cpp src/epoll.cpp)
target_link_libraries(basis_plugins_transport_tcp basis::core::time basis::core::transport basis::core::networking basis::core::threading)
target_include_directories(basis_plugins_transport_tcp PUBLIC include)

add_library(basis::plugins::transport::tcp ALIAS basis_plugins_transport_tcp)

add_subdirectory(test)
