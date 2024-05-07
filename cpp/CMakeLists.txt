cmake_minimum_required(VERSION 3.28)
project(basis)
enable_testing()

# TODO: support lower C++ versions by shimming new libraries with lite/abseil
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # Workaround for std::expected not available in clang
  add_compile_options(
    -stdlib=libstdc++ -D__cpp_concepts=202002 -Wno-builtin-macro-redefined
  )
endif()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)


add_subdirectory(core)

add_subdirectory(plugins/transport/tcp)