cmake_minimum_required(VERSION 3.10)

project(MyStaticFieldProject)

add_library(MyStaticField SHARED SizeHolder.cpp)

target_include_directories(MyStaticField PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(
        MyStaticField
        PROPERTIES
        LINKER_LANGUAGE
        CXX
)

export(TARGETS MyStaticField FILE MyStaticFieldConfig.cmake)
