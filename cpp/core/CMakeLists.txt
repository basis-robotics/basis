project(basis_core)

# basis::core
# Intended for libraries that go into basis::unit
# Not generally intended for end user use, but there are no restrictions against its usage

add_subdirectory(coordinator)
add_subdirectory(containers)
add_subdirectory(networking)
add_subdirectory(serialization)
add_subdirectory(threading)
add_subdirectory(time)
add_subdirectory(transport)
