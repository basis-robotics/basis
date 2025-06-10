if(${BASIS_ENABLE_ROS})
    include(Unit)
    
    find_package(sensor_msgs REQUIRED PATHS ${BASIS_ROS_ROOT}/share/sensor_msgs/cmake)

    generate_unit(pybind_test)

    target_link_libraries(unit_pybind_test basis::plugins::serialization::rosmsg rostime)

    find_package(Python REQUIRED COMPONENTS Development Interpreter)

    add_library(python_shim src/python_shim.c)
    target_include_directories(python_shim PUBLIC include)
    target_link_libraries(python_shim PUBLIC Python::Python)

    target_include_directories(unit_pybind_test PUBLIC ${Python_INCLUDE_DIRS})

    install(TARGETS python_shim DESTINATION unit/)

    uv_add_pyproject(py/pyproject.toml)

endif()