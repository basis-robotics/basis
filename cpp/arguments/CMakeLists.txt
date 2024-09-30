# basis::arguments - Library to add common argparse interface for units and launch files
add_library(basis_arguments SHARED src/argument_types.cpp)

target_include_directories(basis_arguments PUBLIC include)
target_link_libraries(basis_arguments PUBLIC argparse expected-lite nlohmann_json spdlog::spdlog)

add_library(basis::arguments ALIAS basis_arguments)