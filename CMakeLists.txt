cmake_minimum_required(VERSION 3.10)
project(position_distributor)

set(CMAKE_CXX_STANDARD 23)

add_executable(position_distributor server.cpp)
target_include_directories(position_distributor PRIVATE src)

add_subdirectory(test)
