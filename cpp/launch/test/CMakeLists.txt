add_executable(
  test_launch_definition
  test_launch_definition.cpp
)

target_link_libraries(
  test_launch_definition
  basis::launch
  GTest::gtest_main
)

include(GoogleTest REQUIRED)
message(${CMAKE_CURRENT_BINARY_DIR})
gtest_discover_tests(test_launch_definition WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

macro(add_test_file FILE_NAME)
add_custom_target(
  test_launch_definition_${FILE_NAME}
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/${FILE_NAME}
                ${CMAKE_CURRENT_BINARY_DIR}/${FILE_NAME})
  add_dependencies(test_launch_definition test_launch_definition_${FILE_NAME})
endmacro()

add_test_file(test_include.launch.yaml)
add_test_file(foxglove.launch.yaml)
