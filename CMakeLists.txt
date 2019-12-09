# file      : CMakeLists.txt
# copyright : Copyright (c) 2018-present, Lava Block OÜ
# license   : MIT; see accompanying LICENSE file

cmake_minimum_required(VERSION 3.12)

project(liblava-demo VERSION 0.4.5 LANGUAGES C CXX)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/liblava ${CMAKE_CURRENT_BINARY_DIR}/liblava)

set(LIBLAVA_DEMO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/liblava-demo)
set(LIBLAVA_DEMO_EXT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext)

message("> lava-triangle")

add_executable(lava-triangle 
        ${LIBLAVA_DEMO_DIR}/triangle.cpp
        )

target_link_libraries(lava-triangle lava::app)

message("> lava-lamp")

add_executable(lava-lamp 
        ${LIBLAVA_DEMO_DIR}/lamp.cpp
        )

target_link_libraries(lava-lamp lava::app)

message("> lava-spawn")

add_executable(lava-spawn 
        ${LIBLAVA_DEMO_DIR}/spawn.cpp
        )

target_link_libraries(lava-spawn lava::app)
