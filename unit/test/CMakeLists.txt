# WIP unit for adding new features
if(${BASIS_ENABLE_ROS} AND ${BASIS_ENABLE_TESTING})
    include(Unit)

    find_package(sensor_msgs REQUIRED PATHS ${BASIS_ROS_ROOT}/share/sensor_msgs/cmake)

    generate_unit(test_unit)

    # TODO: query the yaml for the types
    target_link_libraries(unit_test_unit basis::plugins::serialization::rosmsg rostime)

    add_subdirectory(test)
endif()