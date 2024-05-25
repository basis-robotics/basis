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
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

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


# Enable warnings after we've included dependencies
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

add_subdirectory(core)

add_subdirectory(plugins/serialization/protobuf)
add_subdirectory(plugins/serialization/rosmsg)

add_subdirectory(plugins/transport/tcp)