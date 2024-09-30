# basis::arguments - Library to add common argparse interface for units and launch files
add_library(basis_arguments INTERFACE)

target_include_directories(basis_arguments INTERFACE include)
target_link_libraries(basis_arguments INTERFACE argparse expected-lite nlohmann_json spdlog::spdlog)

add_library(basis::arguments ALIAS basis_arguments)