project(basis_plugins_transport_tcp)

add_plugin(basis_plugins_transport_tcp src/tcp.cpp src/tcp_subscriber.cpp src/epoll.cpp src/tcp_connection.cpp)
target_link_libraries(basis_plugins_transport_tcp basis::core::time basis::core::transport basis::core::networking basis::core::threading)
target_include_directories(basis_plugins_transport_tcp PUBLIC include)

add_library(basis::plugins::transport::tcp ALIAS basis_plugins_transport_tcp)

if(${BASIS_ENABLE_TESTING})
  add_subdirectory(test)
endif()