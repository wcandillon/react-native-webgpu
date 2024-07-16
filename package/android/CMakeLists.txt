cmake_minimum_required(VERSION 3.4.1)
project(RNWGPU)

set (CMAKE_VERBOSE_MAKEFILE ON)
set (CMAKE_CXX_STANDARD 17)

set (PACKAGE_NAME "react-native-webgpu")

#link_directories(../libs/android/${ANDROID_ABI}/)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG=1)
endif()

if(RNF_ENABLE_LOGS)
    add_definitions(-DRNF_ENABLE_LOGS=1)
else()
    add_definitions(-DRNF_ENABLE_LOGS=0)
endif()

set (WEBGPU_LIBS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../libs/android/${ANDROID_ABI}")

find_package(ReactAndroid REQUIRED CONFIG)
find_package(fbjni REQUIRED CONFIG)

add_library(${PACKAGE_NAME} SHARED
    ./cpp/cpp-adapter.cpp
    ../cpp/rnwgpu/api/GPU.cpp
    ../cpp/rnwgpu/api/GPUAdapter.cpp
    ../cpp/rnwgpu/api/GPUDevice.cpp
    ../cpp/rnwgpu/api/GPUBuffer.cpp
    ../cpp/rnwgpu/api/GPUQueue.cpp
    ../cpp/rnwgpu/api/GPUCommandEncoder.cpp
    ../cpp/rnwgpu/api/GPUQuerySet.cpp
    ../cpp/rnwgpu/api/GPUTexture.cpp
    ../cpp/rnwgpu/api/GPURenderBundleEncoder.cpp
    ../cpp/rnwgpu/api/GPURenderPassEncoder.cpp
    ../cpp/rnwgpu/api/GPURenderPipeline.cpp
    ../cpp/rnwgpu/api/GPUBindGroup.cpp
    ../cpp/rnwgpu/api/GPUComputePassEncoder.cpp
    ../cpp/rnwgpu/api/GPUComputePipeline.cpp
    ../cpp/rnwgpu/RNWebGPUManager.cpp
    ../cpp/jsi/RNFPromise.cpp
    ../cpp/jsi/RNFHybridObject.cpp
    ../cpp/jsi/RNFRuntimeCache.cpp
    ../cpp/jsi/RNFWorkletRuntimeRegistry.cpp
)

target_include_directories(
        ${PACKAGE_NAME}
        PRIVATE

        "${NODE_MODULES_DIR}/react-native/ReactCommon/callinvoker"
        "${NODE_MODULES_DIR}/react-native/ReactCommon/jsi"
        "${NODE_MODULES_DIR}/react-native/ReactCommon"
        "${NODE_MODULES_DIR}/react-native/ReactCommon/react/nativemodule/core"
        "${NODE_MODULES_DIR}/react-native/ReactCommon/runtimeexecutor"
        "${NODE_MODULES_DIR}/react-native/ReactAndroid/src/main/jni"
        "${NODE_MODULES_DIR}/react-native/ReactAndroid/src/main/java/com/facebook/react/turbomodule/core/jni"

        ../cpp
        ../cpp/rnwgpu
        ../cpp/rnwgpu/api
        ../cpp/rnwgpu/api/descriptors
        ../cpp/jsi
        ../cpp/webgpu

        ${libfbjni_include_DIRS}
)

add_library(webgpu_dawn SHARED IMPORTED)
set_property(TARGET webgpu_dawn PROPERTY IMPORTED_LOCATION "${WEBGPU_LIBS_PATH}/libwebgpu_dawn.so")

# Link
target_link_libraries(
        ${PACKAGE_NAME}
        ${FBJNI_LIBRARY}
        log
        jnigraphics
        android
        fbjni::fbjni
        ReactAndroid::jsi
        webgpu_dawn
    )
