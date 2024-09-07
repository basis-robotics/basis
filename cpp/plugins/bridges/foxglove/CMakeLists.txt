project(foxglove_bridge)
include(FetchContent)
include(DeclarePlugin)

add_plugin(basis_plugins_bridges_foxglove src/foxglove_bridge.cpp)

FetchContent_Declare(
  foxglove_websocket
  URL https://github.com/foxglove/ws-protocol/archive/refs/tags/releases/cpp/v1.3.0.zip
)
FetchContent_MakeAvailable(foxglove_websocket)

add_subdirectory(
  ${foxglove_websocket_SOURCE_DIR}/cpp/foxglove-websocket
  ${CMAKE_BINARY_DIR}/foxglove-websocket
)

target_compile_definitions(basis_plugins_bridges_foxglove PRIVATE ASIO_STANDALONE)
target_compile_definitions(foxglove_websocket PUBLIC ASIO_STANDALONE)

target_include_directories(basis_plugins_bridges_foxglove PUBLIC
    ${foxglove_websocket_SOURCE_DIR}/cpp/foxglove-websocket/include
)

target_link_libraries(basis_plugins_bridges_foxglove
  foxglove_websocket
  basis::core::logging
  basis::core::threading
  basis::plugins::transport::tcp
  basis::unit
)
target_include_directories(basis_plugins_bridges_foxglove PUBLIC include)

add_library(basis::plugins::bridges::foxglove ALIAS basis_plugins_bridges_foxglove)
