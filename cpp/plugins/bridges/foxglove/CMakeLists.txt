project(foxglove_bridge)
include(FetchContent)

add_plugin(basis_plugins_bridges_foxglove src/foxglove_bridge.cpp)
#find_package(foxglove_websocket)
#check_include_file_cxx()
find_path (foxglove_websocket_include foxglove/websocket/websocket_server.hpp)
if(NOT foxglove_websocket_include)
  FetchContent_Declare(
    foxglove_websocket
    PATCH_COMMAND sed -i -e "s/CMAKE_SOURCE_DIR/PROJECT_SOURCE_DIR/g" cpp/foxglove-websocket/CMakeLists.txt
    URL https://github.com/foxglove/ws-protocol/archive/refs/tags/releases/cpp/v1.3.0.zip
    FIND_PACKAGE_ARGS NAMES foxglove_websocket
  )
  FetchContent_MakeAvailable(foxglove_websocket)

  add_subdirectory(
    ${foxglove_websocket_SOURCE_DIR}/cpp/foxglove-websocket
    ${CMAKE_BINARY_DIR}/foxglove-websocket
  )
  set_target_properties(foxglove_websocket PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
  target_compile_definitions(foxglove_websocket PUBLIC ASIO_STANDALONE)
endif()
find_package(OpenSSL)

target_compile_definitions(basis_plugins_bridges_foxglove PRIVATE ASIO_STANDALONE)

target_include_directories(basis_plugins_bridges_foxglove PUBLIC
    ${foxglove_websocket_SOURCE_DIR}/cpp/foxglove-websocket/include
)

target_link_libraries(basis_plugins_bridges_foxglove
  foxglove_websocket
  basis::core::logging
  basis::core::threading
  basis::plugins::transport::tcp
  basis::unit
  OpenSSL::SSL
)
target_include_directories(basis_plugins_bridges_foxglove PUBLIC include)

set_target_properties(basis_plugins_bridges_foxglove PROPERTIES OUTPUT_NAME "foxglove.unit")
set_target_properties(basis_plugins_bridges_foxglove PROPERTIES PREFIX "")

install(TARGETS basis_plugins_bridges_foxglove DESTINATION unit/)

add_library(basis::plugins::bridges::foxglove ALIAS basis_plugins_bridges_foxglove)

add_executable(foxglove_bridge src/foxglove_dummy_main.cpp)
target_link_libraries(foxglove_bridge basis_plugins_bridges_foxglove basis::unit::main)
install(TARGETS foxglove_bridge DESTINATION bin/)