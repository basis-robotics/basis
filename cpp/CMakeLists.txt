# TODO: support lower C++ versions by shimming new libraries with lite/abseil
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD 20 PARENT_SCOPE)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON PARENT_SCOPE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON PARENT_SCOPE)
# Work around old cmake on arm64 (or possibly too new dependency)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

# Enforce c++20 even for FetchContent dependencies
# This is only needed under l4t cuda image, which changes headers around in a weird manner
add_compile_options("--std=c++20")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Workaround for std::expected not available in clang
  add_compile_options(
    -D__cpp_concepts=202002 -Wno-builtin-macro-redefined
  )
  # For now, work around issue with protobuf on ubuntu 20 using std::is_pod
  add_compile_options(-Wno-deprecated-declarations)
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
  expected-lite
  URL https://github.com/martinmoene/expected-lite/archive/refs/tags/v0.6.3.zip
)
FetchContent_MakeAvailable(expected-lite)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp/
  GIT_TAG 0.8.0
)
FetchContent_MakeAvailable(yaml-cpp)

FetchContent_Declare(
  nlohmann_json
  OVERRIDE_FIND_PACKAGE
  GIT_REPOSITORY https://github.com/nlohmann/json/
  GIT_TAG v3.10.5
)
set(JSON_MultipleHeaders ON CACHE INTERNAL "force json to use multiple headers to allow for forward declarations")
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
  inja
  GIT_REPOSITORY https://github.com/pantor/inja.git
  GIT_TAG v3.4.0
)
set(BUILD_BENCHMARK OFF CACHE INTERNAL "force inja benchmarks to off")
set(INJA_BUILD_TESTS OFF CACHE INTERNAL "force inja benchmarks to off")
set(INJA_USE_EMBEDDED_JSON OFF CACHE INTERNAL "we will supply json to inja")
FetchContent_MakeAvailable(inja)

FetchContent_Declare(mcap
  GIT_REPOSITORY https://github.com/foxglove/mcap
  GIT_TAG releases/cpp/v2.0.2
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  )

# mcap doesn't contain a CMakeLists.txt, so create an interface library ourselves
FetchContent_MakeAvailable(mcap)
add_library(mcap INTERFACE)
target_include_directories(mcap INTERFACE ${mcap_SOURCE_DIR}/cpp/mcap/include)

set(ARGPARSE_BUILD_TESTS OFF CACHE INTERNAL "force argparse tests to off")
FetchContent_Declare(
  argparse
  GIT_REPOSITORY https://github.com/p-ranav/argparse.git
  GIT_TAG v3.1
)
FetchContent_MakeAvailable(argparse)

# Also requires one of: libbfd (gnu binutils), libdwarf, libdw (elfutils)
FetchContent_Declare(backward
  SYSTEM    
  GIT_REPOSITORY https://github.com/bombela/backward-cpp
  GIT_TAG master  # or a version tag, such as v1.6
  PATCH_COMMAND sed -i -e "s/set(CMAKE_CXX_STANDARD 11)/#set(CMAKE_CXX_STANDARD 11)/g" CMakeLists.txt
)
FetchContent_MakeAvailable(backward)

if(BASIS_ENABLE_ROS)
  set(BUILD_TESTING OFF)
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

add_subdirectory(arguments)
add_subdirectory(core)
add_subdirectory(cli)
add_subdirectory(examples)
add_subdirectory(launch)
add_subdirectory(recorder)
add_subdirectory(replayer)
add_subdirectory(synchronizers)
add_subdirectory(unit)

add_subdirectory(plugins/serialization/protobuf)
if(BASIS_ENABLE_ROS)
  add_subdirectory(plugins/serialization/rosmsg)
endif()

add_subdirectory(plugins/transport/tcp)
add_subdirectory(plugins/bridges/foxglove)

add_subdirectory(third_party)
