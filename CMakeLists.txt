cmake_minimum_required(VERSION 3.28)
project(basis)

# Allow include() on our cmake directory
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(BASIS_SOURCE_ROOT ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL STRING)
message(STATUS "BASIS_SOURCE_ROOT: ${BASIS_SOURCE_ROOT}")

#################################################################################
# Options
set(BASIS_INSTALL_DIR /opt/basis CACHE STRING "Directory to install basis to")
option(BASIS_ENABLE_ROS "Enable basis support for ROS" OFF)
option(BASIS_ENABLE_TESTING "Build tests" ${PROJECT_IS_TOP_LEVEL})

# Set variables based on options
set(CMAKE_INSTALL_PREFIX ${BASIS_INSTALL_DIR})

if(${BASIS_ENABLE_TESTING})
    enable_testing()
endif()

if(BASIS_ENABLE_ROS)
    set(BASIS_ROS_ROOT /opt/ros/noetic CACHE STRING "ros root to use")
    add_compile_definitions(BASIS_ENABLE_ROS=1)
endif()
#################################################################################

# Set global settings
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add all of our supported languages
add_subdirectory(cpp)
add_subdirectory(proto)

# Add the units
add_subdirectory(unit)

include(FindAllTargets)
find_all_targets(ALL_TARGETS)

# For each binary and shared object, install
# TODO: we probably want https://cmake.org/cmake/help/latest/guide/importing-exporting/index.html#exporting-targets-from-the-build-tree
# TODO: headers aren't installed this way
foreach(TARGET IN LISTS ALL_TARGETS)
get_target_property(TARGET_TYPE ${TARGET} TYPE)
    if(${TARGET_TYPE} STREQUAL "EXECUTABLE")
        install(TARGETS ${TARGET})
    elseif(${TARGET_TYPE} STREQUAL "SHARED_LIBRARY")
        get_target_property(PLUGIN_TYPE ${TARGET} BASIS_PLUGIN_TYPE)

        # TODO: plugins are special
        if("${PLUGIN_TYPE}" STREQUAL "PLUGIN_TYPE-NOTFOUND")
            install(TARGETS ${TARGET})
        endif()
    endif()
endforeach()
