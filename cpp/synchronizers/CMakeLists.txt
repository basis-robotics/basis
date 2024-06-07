add_library(basis_synchronizers STATIC src/synchronizers.cpp)
target_include_directories(basis_synchronizers PUBLIC include)

add_library(basis::synchronizers ALIAS basis_synchronizers)

if(${BASIS_ENABLE_TESTING})
  add_subdirectory(test)
endif()