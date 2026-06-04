cmake_minimum_required(VERSION 3.4.1)
project(RNWGPU)

set (CMAKE_VERBOSE_MAKEFILE ON)
set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED True)

set (PACKAGE_NAME "react-native-wgpu")

#link_directories(../libs/android/${ANDROID_ABI}/)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG=1)
endif()

if(RNF_ENABLE_LOGS)
    add_definitions(-DRNF_ENABLE_LOGS=1)
else()
    add_definitions(-DRNF_ENABLE_LOGS=0)
endif()

# "Bring your own Dawn" mode. When another module (e.g. Skia Graphite) already
# links a Dawn build into the app, set RN_WEBGPU_EXTERNAL_DAWN=ON to skip
# bundling our own libwebgpu_dawn.so. In that mode react-native-wgpu links
# nothing Dawn-related of its own; its Dawn symbols come from the host.
#   - RN_WEBGPU_DAWN_HEADERS: directory containing the `webgpu/` header folder
#     (Dawn's include dir). Prepended so the host's headers win over our copy.
#   - RN_WEBGPU_DAWN_LIB: path to the host's Dawn shared library to link
#     against. If left empty, undefined Dawn symbols are allowed and resolved
#     from the host at load time.
option(RN_WEBGPU_EXTERNAL_DAWN "Use a host-provided Dawn instead of the bundled one" OFF)
set(RN_WEBGPU_DAWN_HEADERS "" CACHE STRING "Path to host Dawn headers (dir containing webgpu/)")
set(RN_WEBGPU_DAWN_LIB "" CACHE STRING "Path to host Dawn shared library (external mode)")

set (WEBGPU_LIBS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../libs/android/${ANDROID_ABI}")

find_package(ReactAndroid REQUIRED CONFIG)
find_package(fbjni REQUIRED CONFIG)

add_library(${PACKAGE_NAME} SHARED
    ./cpp/cpp-adapter.cpp
    ../cpp/rnwgpu/api/GPU.cpp
    ../cpp/rnwgpu/api/GPUAdapter.cpp
    ../cpp/rnwgpu/api/GPUSupportedLimits.cpp
    ../cpp/rnwgpu/api/GPUShaderModule.cpp
    ../cpp/rnwgpu/api/GPUDeviceLostInfo.cpp
    ../cpp/rnwgpu/api/GPUDevice.cpp
    ../cpp/rnwgpu/api/GPUBuffer.cpp
    ../cpp/rnwgpu/api/GPUQueue.cpp
    ../cpp/rnwgpu/api/GPUCommandEncoder.cpp
    ../cpp/rnwgpu/api/GPUQuerySet.cpp
    ../cpp/rnwgpu/api/GPUTexture.cpp
    ../cpp/rnwgpu/api/GPUSharedTextureMemory.cpp
    ../cpp/rnwgpu/api/GPUExternalTexture.cpp
    ../cpp/rnwgpu/api/GPURenderBundleEncoder.cpp
    ../cpp/rnwgpu/api/GPURenderPassEncoder.cpp
    ../cpp/rnwgpu/api/GPURenderPipeline.cpp
    ../cpp/rnwgpu/api/GPUBindGroup.cpp
    ../cpp/rnwgpu/api/GPUComputePassEncoder.cpp
    ../cpp/rnwgpu/api/GPUComputePipeline.cpp
    ../cpp/rnwgpu/api/GPUCanvasContext.cpp
    ../cpp/rnwgpu/RNWebGPUManager.cpp
    ../cpp/jsi/Promise.cpp
    ../cpp/jsi/RuntimeLifecycleMonitor.cpp
    ../cpp/jsi/RuntimeAwareCache.cpp
    ../cpp/rnwgpu/async/AsyncRunner.cpp
    ../cpp/rnwgpu/async/AsyncTaskHandle.cpp
    ../cpp/rnwgpu/async/JSIMicrotaskDispatcher.cpp
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
        "${NODE_MODULES_DIR}/react-native/ReactAndroid/src/main/jni/react/turbomodule"

        ../cpp
        ../cpp/rnwgpu
        ../cpp/rnwgpu/api
        ../cpp/rnwgpu/api/descriptors
        ../cpp/jsi
        ../cpp/webgpu
        ../cpp/platform

        ${libfbjni_include_DIRS}
)

if(RN_WEBGPU_EXTERNAL_DAWN)
    message(STATUS "react-native-wgpu: external Dawn mode (not bundling libwebgpu_dawn)")
    if(RN_WEBGPU_DAWN_HEADERS)
        # Prepend so the host's <webgpu/webgpu.h> wins over our bundled copy.
        target_include_directories(${PACKAGE_NAME} BEFORE PRIVATE "${RN_WEBGPU_DAWN_HEADERS}")
    endif()
    if(RN_WEBGPU_DAWN_LIB)
        add_library(webgpu_dawn SHARED IMPORTED)
        set_property(TARGET webgpu_dawn PROPERTY IMPORTED_LOCATION "${RN_WEBGPU_DAWN_LIB}")
        set(WEBGPU_DAWN_LINK webgpu_dawn)
    else()
        # No host library given at build time: leave Dawn symbols undefined and
        # let the dynamic linker resolve them from the host at load time.
        target_link_options(${PACKAGE_NAME} PRIVATE "-Wl,--allow-shlib-undefined")
        set(WEBGPU_DAWN_LINK "")
    endif()
else()
    add_library(webgpu_dawn SHARED IMPORTED)
    set_property(TARGET webgpu_dawn PROPERTY IMPORTED_LOCATION "${WEBGPU_LIBS_PATH}/libwebgpu_dawn.so")
    set(WEBGPU_DAWN_LINK webgpu_dawn)
endif()

# Link
if(ReactAndroid_VERSION_MINOR GREATER_EQUAL 76)
    target_link_libraries(
            ${PACKAGE_NAME}
            ${FBJNI_LIBRARY}
            log
            jnigraphics
            android
            fbjni::fbjni
            ReactAndroid::jsi
            ReactAndroid::reactnative
            ${WEBGPU_DAWN_LINK}
        )
else()
    target_link_libraries(
        ${PACKAGE_NAME}
        ${FBJNI_LIBRARY}
        log
        jnigraphics
        android
        fbjni::fbjni
        ReactAndroid::jsi
        ReactAndroid::reactnativejni
        ${WEBGPU_DAWN_LINK}
    )
endif()
