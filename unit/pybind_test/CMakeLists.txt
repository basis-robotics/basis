if(${BASIS_PYTHON_VERSION})

if(${BASIS_ENABLE_ROS})
    include(Unit)
    find_package(Python ${BASIS_PYTHON_VERSION} REQUIRED COMPONENTS Development Interpreter)

    find_package(sensor_msgs REQUIRED PATHS ${BASIS_ROS_ROOT}/share/sensor_msgs/cmake)

    generate_unit(pybind_test)

    # Note: this MUST use private linkage for python, otherwise the
    # _bin target will also need rpath tweaking
    target_link_libraries(unit_pybind_test PUBLIC basis::plugins::serialization::rosmsg rostime)# PRIVATE Python::Python)
    target_link_options(unit_pybind_test PUBLIC "-Wl,--allow-shlib-undefined")
    # target_link_options(unit_pybind_test PRIVATE "-Wl,--allow-shlib-undefined")

    # Set RPATH of pybind test to point back at the version of python we are using
    # This is needed w/ uv as the python so doesn't get added to LD_LIBRARY_PATH
    # TODO: this might not be quite right if we want to use a different python lib at runtime
    # TODO: this is completely wrong, need to rewrite the 
    # set_property(TARGET unit_pybind_test PROPERTY INSTALL_RPATH "${Python_RUNTIME_LIBRARY_DIRS}")

    

    target_include_directories(unit_pybind_test PUBLIC include ${Python_INCLUDE_DIRS})

    # TODO: move this up and set the correct RPATH
    uv_add_pyproject(py/pyproject.toml)

endif()

endif()