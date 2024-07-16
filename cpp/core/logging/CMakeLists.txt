add_library(basis_core_logging SHARED src/logging.cpp)

target_include_directories(basis_core_logging PUBLIC include)
target_link_libraries(basis_core_logging
  basis::core::time
  spdlog)
add_library(basis::core::logging ALIAS basis_core_logging)
