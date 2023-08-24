cmake_minimum_required(VERSION 3.26)
project(BLDownloader)

set(CMAKE_CXX_STANDARD 17)
link_libraries(ws2_32 wsock32)

add_executable(BLDownloader main.cpp
        trim.h)
