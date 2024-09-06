cmake_minimum_required(VERSION 3.10)

project(StaticDataBridge)

add_library(StaticDataBridge SHARED SizeHolder.cpp)

target_include_directories(StaticDataBridge PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(
        StaticDataBridge
        PROPERTIES
        LINKER_LANGUAGE
        CXX
)

export(TARGETS StaticDataBridge FILE StaticDataBridgeConfig.cmake)
