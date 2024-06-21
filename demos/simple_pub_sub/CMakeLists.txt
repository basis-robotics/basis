cmake_minimum_required(VERSION 3.28)
project(SimplePubSub)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

target_link_libraries(Basis PUBLIC ../../cpp)
target_include_directories(SimplePubSub PUBLIC
                           "../../cpp")

add_executable(SimplePubSub src/simple_pub_sub.cpp)
