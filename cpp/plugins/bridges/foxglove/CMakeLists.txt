project(foxglove_bridge)
include(FetchContent)

add_library(foxglove_bridge SHARED
  src/foxglove_bridge.cpp
)

FetchContent_Declare(
  foxglove_websocket
  URL https://github.com/foxglove/ws-protocol/archive/refs/tags/releases/cpp/v1.3.0.zip
)
FetchContent_MakeAvailable(foxglove_websocket)

add_subdirectory(
  ${foxglove_websocket_SOURCE_DIR}/cpp/foxglove-websocket
  ${CMAKE_BINARY_DIR}/foxglove-websocket
)

target_compile_definitions(foxglove_bridge PRIVATE ASIO_STANDALONE)
target_compile_definitions(foxglove_websocket PUBLIC ASIO_STANDALONE)

target_include_directories(foxglove_bridge PRIVATE
    ${foxglove_websocket_SOURCE_DIR}/cpp/foxglove-websocket/include
)

target_link_libraries(foxglove_bridge
  foxglove_websocket
  basis::core::logging
  basis::core::threading
  basis::plugins::transport::tcp
  basis::unit
)
