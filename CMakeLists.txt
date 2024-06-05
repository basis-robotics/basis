cmake_minimum_required(VERSION 3.28)
project(basis)

set(BASIS_INSTALL_DIR /opt/basis CACHE STRING "Directory to install basis to")
set(CMAKE_INSTALL_PREFIX ${BASIS_INSTALL_DIR})

option(BASIS_ENABLE_TESTING "Build tests" ${PROJECT_IS_TOP_LEVEL})
if(${BASIS_ENABLE_TESTING})
    enable_testing()
endif()

option(BASIS_ENABLE_ROS "Enable basis support for ROS" OFF)
if(BASIS_ENABLE_ROS)
    set(BASIS_ROS_ROOT /opt/ros/noetic CACHE STRING "ros root to use")
    add_compile_definitions(BASIS_ENABLE_ROS=1)
endif()


set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(cpp)
add_subdirectory(proto)

# Utilities to get all targets of our build
function(get_all_targets var)
    set(targets)
    get_all_targets_recursive(targets ${CMAKE_CURRENT_SOURCE_DIR})
    set(${var} ${targets} PARENT_SCOPE)
endfunction()

macro(get_all_targets_recursive targets dir)
    get_property(subdirectories DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(subdir ${subdirectories})
        get_all_targets_recursive(${targets} ${subdir})
    endforeach()

    get_property(current_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
    list(APPEND ${targets} ${current_targets})
endmacro()

get_all_targets(ALL_TARGETS)

# For each binary and shared object, install
# TODO: we probably want https://cmake.org/cmake/help/latest/guide/importing-exporting/index.html#exporting-targets-from-the-build-tree
# TODO: headers aren't installed this way
foreach(TARGET IN LISTS ALL_TARGETS)
get_target_property(TARGET_TYPE ${TARGET} TYPE)
    if(${TARGET_TYPE} STREQUAL "EXECUTABLE")
        install(TARGETS ${TARGET})
    elseif(${TARGET_TYPE} STREQUAL "SHARED_LIBRARY")
        # TODO: plugins are special
        install(TARGETS ${TARGET})
    endif()
endforeach()
