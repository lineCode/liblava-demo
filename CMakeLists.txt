# file      : CMakeLists.txt
# copyright : Copyright (c) 2018-present, Lava Block OÜ
# license   : MIT; see accompanying LICENSE file

cmake_minimum_required(VERSION 3.12)

project(liblava-demo VERSION 0.4.4 LANGUAGES C CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/liblava ${CMAKE_CURRENT_BINARY_DIR}/liblava)

message("> lava-lamp")

add_executable(lava-lamp 
        ${CMAKE_CURRENT_SOURCE_DIR}/lamp/lamp.cpp
        )

target_link_libraries(lava-lamp lava::tool)

message("> lava-triangle")

add_executable(lava-triangle 
        ${CMAKE_CURRENT_SOURCE_DIR}/triangle/triangle.cpp
        )

target_link_libraries(lava-triangle lava::tool)

message("> lava-spawn")

add_executable(lava-spawn 
        ${CMAKE_CURRENT_SOURCE_DIR}/spawn/spawn.cpp
        )

target_link_libraries(lava-spawn lava::tool)
