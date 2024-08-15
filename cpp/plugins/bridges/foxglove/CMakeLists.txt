project(foxglove_bridge)
# include(DeclarePlugin)

# find_package(foxglove_bridge_base REQUIRED)


add_library(foxglove_bridge SHARED
  src/foxglove_bridge.cpp
)

target_link_libraries(foxglove_bridge
  foxglove_bridge_base
)


# add_plugin(basis_plugins_bridges_foxglove src/foxglove_bridge.cpp)
# target_include_directories(basis_plugins_bridges_foxglove PUBLIC include)

# add_library(basis::plugins::bridges::foxglove ALIAS basis_plugins_bridges_foxglove)
