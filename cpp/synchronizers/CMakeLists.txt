add_library(basis_synchronizers INTERFACE)

target_include_directories(basis_synchronizers INTERFACE include)

add_library(basis::synchronizers ALIAS basis_synchronizers)

if(${BASIS_ENABLE_TESTING})
  add_subdirectory(test)
endif()