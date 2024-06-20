cmake_minimum_required(VERSION 3.4.1)
project(RNWGPU)

set (CMAKE_VERBOSE_MAKEFILE ON)
set (CMAKE_CXX_STANDARD 17)

set (PACKAGE_NAME "react-native-webgpu")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG=1)
endif()

if(RNF_ENABLE_LOGS)
    add_definitions(-DRNF_ENABLE_LOGS=1)
else()
    add_definitions(-DRNF_ENABLE_LOGS=0)
endif()

add_library(${PACKAGE_NAME} SHARED
    ./cpp/cpp-adapter.cpp
    ./cpp/RNFAndroidLogger.cpp
    ../cpp/rnwgpu/RNWebGPUManager.cpp
    ../cpp/rnwgpu/api/GPU.cpp
)

# Specifies a path to native header files.
include_directories(
  ../cpp
  ../cpp/rnwgpu
  ../cpp/rnwgpu/api
  ../cpp/jsi
  ../cpp/dawn
  ../cpp/webgpu
)

# Link
target_link_libraries(
        ${PACKAGE_NAME}
        ${LOG_LIB} 
        -llog
        -ljnigraphics
        -landroid
    )
