project(basis_core_transport)

add_library(basis_core_transport SHARED src/inproc.cpp src/publisher.cpp src/subscriber.cpp)
target_link_libraries(basis_core_transport basis::core::serialization basis::core::time basis::core::threading spdlog uuid basis_proto)
target_include_directories(basis_core_transport PUBLIC include)

add_library(basis::core::transport ALIAS basis_core_transport)

add_subdirectory(test)
