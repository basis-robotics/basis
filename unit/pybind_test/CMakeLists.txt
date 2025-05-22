# WIP unit for adding new features
if(${BASIS_ENABLE_ROS})
    include(Unit)
    set (PYBIND11_PYTHON_VERSION "3.8")

    find_package(sensor_msgs REQUIRED PATHS ${BASIS_ROS_ROOT}/share/sensor_msgs/cmake)

    generate_unit(pybind_test)

    # TODO: query the yaml for the types
    target_link_libraries(unit_pybind_test basis::plugins::serialization::rosmsg rostime)



    find_package(Python REQUIRED COMPONENTS Development)
    target_include_directories(unit_pybind_test PUBLIC ${Python_INCLUDE_DIRS})
    target_link_libraries(unit_pybind_test dl)
    #pybind11_add_module(basis_unit_python src/pybind.cpp SHARED)
    target_link_libraries(unit_pybind_test pybind11::headers)# basis_unit_python)
    
#    get_target_property(libs pybind11::embed INTERFACE_LINK_LIBRARIES)
#    message(FATAL_ERROR currlient_libs)

    # filter out unwanted libraries
    list(REMOVE_ITEM libs ${Python_LIBRARIES})
    #list(REMOVE_ITEM current_libs "path/to/object_file.o")
    #set_target_properties(my_target PROPERTIES LINK_LIBRARIES "${current_libs}")


endif()