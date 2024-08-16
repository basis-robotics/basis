project(foxglove_bridge)
# include(DeclarePlugin)

add_library(foxglove_bridge SHARED
  src/foxglove_bridge.cpp
)

target_link_libraries(foxglove_bridge
  foxglove_bridge_base
  basis::core::logging
  basis::core::threading
  basis::plugins::transport::tcp
)


# add_plugin(basis_plugins_bridges_foxglove src/foxglove_bridge.cpp)
# target_include_directories(basis_plugins_bridges_foxglove PUBLIC include)

# add_library(basis::plugins::bridges::foxglove ALIAS basis_plugins_bridges_foxglove)
