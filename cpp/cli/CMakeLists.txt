add_library(basis_cli_logger
  src/cli_logger.cpp)

target_include_directories(basis_cli_logger PUBLIC include)

target_link_libraries(basis_cli_logger basis::core::logging)
add_library(basis::cli_logger ALIAS basis_cli_logger)

add_executable(
  basis
  src/cli.cpp
)

target_link_libraries(
  basis
  basis::launch
  basis::plugins::transport::tcp
  basis::core::coordinator
  argparse
)
