project(foxglove_bridge_base)

find_package(nlohmann_json QUIET)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(websocketpp REQUIRED)
find_package(ZLIB REQUIRED)

option(USE_ASIO_STANDALONE "Build with standalone ASIO" ON)
if(USE_ASIO_STANDALONE)
  message(STATUS "Using standalone ASIO")
  add_definitions(-DASIO_STANDALONE)
else()
# not supporting Boost ASIO for now
#   message(STATUS "Using Boost ASIO")
#   find_package(Boost REQUIRED)
endif(USE_ASIO_STANDALONE)

# Detect big-endian architectures
include(TestBigEndian)
TEST_BIG_ENDIAN(ENDIAN)
if (ENDIAN)
  add_compile_definitions(ARCH_IS_BIG_ENDIAN=1)
endif()

set(FOXGLOVE_BRIDGE_VERSION "${CMAKE_PROJECT_VERSION}")
configure_file(src/version.cpp.in
               src/version.cpp @ONLY)

# Build the foxglove_bridge_base library
add_library(foxglove_bridge_base SHARED
# add_library(basis_plugins_bridges_fose64.cp_bridge_basep
  src/foxglove_bridge.cpp
  src/parameter.cpp
  src/serialization.cpp
  src/server_factory.cpp
  src/test/test_client.cpp
  # Generated:
  ${CMAKE_CURRENT_BINARY_DIR}/src/version.cpp
)
add_library(foxglove::foxglove_bridge_base ALIAS foxglove_bridge_base)

# target_include_directories(foxglove_bridge_base
#   PUBLIC
#     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
#     $<INSTALL_INTERFACE:include>
# )

target_include_directories(foxglove_bridge_base PUBLIC include)

target_link_libraries(foxglove_bridge_base
  OpenSSL::Crypto
  OpenSSL::SSL
  ZLIB::ZLIB
  ${CMAKE_THREAD_LIBS_INIT}
  basis::core::logging
)

if(nlohmann_json_FOUND)
  target_link_libraries(foxglove_bridge_base nlohmann_json::nlohmann_json)
else()
  message(STATUS "nlohmann_json not found, will search at compile time")
endif()
