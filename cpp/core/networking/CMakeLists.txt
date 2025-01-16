project(basis_core_networking)

add_library(basis_core_networking SHARED src/logger.cpp src/socket.cpp)
target_link_libraries(basis_core_networking basis::core::logging nonstd::expected-lite)
target_include_directories(basis_core_networking PUBLIC include)

add_library(basis::core::networking ALIAS basis_core_networking)

if(${BASIS_ENABLE_TESTING})
  add_subdirectory(test)
endif()
