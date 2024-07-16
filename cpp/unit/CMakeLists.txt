project(basis_core_unit)

add_library(basis_unit SHARED src/unit.cpp)
target_include_directories(basis_unit PUBLIC include)
target_link_libraries(basis_unit
    basis::core::coordinator
    basis::core::logging
    basis::core::threading
    basis::plugins::transport::tcp
    )

add_library(basis::unit ALIAS basis_unit)

add_library(basis_unit_main SHARED src/unit_main.cpp)
target_link_libraries(basis_unit_main basis::unit)

add_library(basis::unit::main ALIAS basis_unit_main)
