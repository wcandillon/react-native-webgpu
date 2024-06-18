cmake_minimum_required(VERSION 3.4.1)
project(RNWGPU)

set (CMAKE_VERBOSE_MAKEFILE ON)
set (CMAKE_CXX_STANDARD 14)

set (PACKAGE_NAME "react-native-webgpu")

add_library(${PACKAGE_NAME}            SHARED
            ../cpp/rnwgpu/RNWebGPUManager.cpp
            cpp/cpp-adapter.cpp
)

# Specifies a path to native header files.
include_directories(
  ../cpp
)

# Link
target_link_libraries(
        ${PACKAGE_NAME}
        -ljnigraphics
        -llog
        -landroid
    )
