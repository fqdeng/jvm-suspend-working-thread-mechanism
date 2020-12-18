cmake_minimum_required(VERSION 3.17)
project(mprotect C)

set(CMAKE_C_STANDARD 11)
set(LOCAL_CXX_FLAGS "-fPIC -Os")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LOCAL_CXX_FLAGS}")

add_executable(mprotect main.c)