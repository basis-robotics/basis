project(basis_core_transport)

add_library(basis_core_transport SHARED src/inproc.cpp src/publisher.cpp src/subscriber.cpp)
target_link_libraries(basis_core_transport basis::core::serialization basis::core::time basis::core::threading basis::core::containers spdlog uuid basis_proto)
target_include_directories(basis_core_transport PUBLIC include)

add_library(basis::core::transport ALIAS basis_core_transport)

if(${BASIS_ENABLE_TESTING})
  add_subdirectory(test)
endif()