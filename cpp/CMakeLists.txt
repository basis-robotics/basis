# TODO: support lower C++ versions by shimming new libraries with lite/abseil
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Workaround for std::expected not available in clang
  add_compile_options(
    -D__cpp_concepts=202002 -Wno-builtin-macro-redefined
  )
endif()

include(FetchContent)
if(${BASIS_ENABLE_TESTING})
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
  )
  FetchContent_MakeAvailable(googletest)
endif()

FetchContent_Declare(
  spdlog
  URL  https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.zip
)
FetchContent_MakeAvailable(spdlog)
target_compile_options(spdlog PUBLIC -fPIC)

FetchContent_Declare(
  expected-lite
  URL https://github.com/martinmoene/expected-lite/archive/refs/tags/v0.6.3.zip
)
FetchContent_MakeAvailable(expected-lite)

set(ARGPARSE_BUILD_TESTS OFF CACHE INTERNAL "force argparse tests to off")
FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
  GIT_TAG v3.0
)
FetchContent_MakeAvailable(argparse)

if(BASIS_ENABLE_ROS)
  # TODO: consider using embag instead
  FetchContent_Declare(
    fastcdr
    SYSTEM
    GIT_REPOSITORY https://github.com/eProsima/Fast-CDR
    GIT_TAG v2.2.1
  )
  FetchContent_MakeAvailable(fastcdr)

  # TODO: this may muck with BUILD_SHARED_LIBS
  FetchContent_Declare(
    rosx_introspection
    SYSTEM
    # Needed to fork to work around cmake issues with fetchcontent
    GIT_REPOSITORY https://github.com/basis-robotics/rosx_introspection/
    # No releases :(
    GIT_TAG 72cffd4ce08fe6b3aac434ed31a2e6404a18e4b4
  )
  FetchContent_MakeAvailable(rosx_introspection)
  target_compile_options(rosx_introspection PRIVATE -Wno-implicit-const-int-float-conversion)
  target_link_libraries(rosx_introspection fastcdr)


  find_package(cpp_common REQUIRED PATHS ${BASIS_ROS_ROOT}/share/cpp_common/cmake)
  find_package(rostime REQUIRED PATHS ${BASIS_ROS_ROOT}/share/rostime/cmake)
  find_package(roscpp_traits REQUIRED PATHS ${BASIS_ROS_ROOT}/share/roscpp_traits/cmake)
  find_package(roscpp_serialization REQUIRED PATHS ${BASIS_ROS_ROOT}/share/roscpp_serialization/cmake)
  find_package(message_runtime REQUIRED PATHS ${BASIS_ROS_ROOT}/share/message_runtime/cmake)
endif()


# Enable warnings after we've included dependencies
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

add_subdirectory(core)
add_subdirectory(cli)
add_subdirectory(examples)
add_subdirectory(synchronizers)
add_subdirectory(unit)

add_subdirectory(plugins/serialization/protobuf)
if(BASIS_ENABLE_ROS)
  add_subdirectory(plugins/serialization/rosmsg)
endif()

add_subdirectory(plugins/transport/tcp)