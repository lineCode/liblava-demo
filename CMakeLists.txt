# file      : CMakeLists.txt
# copyright : Copyright (c) 2018-present, Lava Block OÜ
# license   : MIT; see accompanying LICENSE file

cmake_minimum_required(VERSION 3.12)

project(liblava-demo VERSION 0.4.3 LANGUAGES C CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/liblava ${CMAKE_CURRENT_BINARY_DIR}/liblava)

message(">> lava-lamp")

add_executable(lava-lamp 
        ${CMAKE_CURRENT_SOURCE_DIR}/lamp/lamp.cpp
        )

target_link_libraries(lava-lamp lava::tool)
