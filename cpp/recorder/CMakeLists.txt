add_library(basis_recorder SHARED src/recorder.cpp)

target_include_directories(basis_recorder PUBLIC include)
target_link_libraries(basis_recorder basis::core::time basis::core::containers mcap spdlog)
add_library(basis::recorder ALIAS basis_recorder)

if(${BASIS_ENABLE_TESTING})
  add_subdirectory(test)
endif()