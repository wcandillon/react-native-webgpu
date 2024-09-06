cmake_minimum_required(VERSION 3.4.1)
project(RNWGPU)
MESSAGE(WARNING "mleko2")
set (CMAKE_VERBOSE_MAKEFILE ON)
set (CMAKE_CXX_STANDARD 17)

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

set (WEBGPU_LIBS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../libs/android/${ANDROID_ABI}")

find_package(ReactAndroid REQUIRED CONFIG)

MESSAGE(WARNING dupa1 ${ReactAndroid_CONSIDERED_CONFIGS})

MESSAGE(WARNING dupa2 ${ReactAndroid_CONFIG})

find_package(fbjni REQUIRED CONFIG)

add_library(${PACKAGE_NAME} SHARED
    ./cpp/cpp-adapter.cpp
#        SizeHolder/SizeHolder.cpp
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
    ../cpp/rnwgpu/api/GPURenderBundleEncoder.cpp
    ../cpp/rnwgpu/api/GPURenderPassEncoder.cpp
    ../cpp/rnwgpu/api/GPURenderPipeline.cpp
    ../cpp/rnwgpu/api/GPUBindGroup.cpp
    ../cpp/rnwgpu/api/GPUComputePassEncoder.cpp
    ../cpp/rnwgpu/api/GPUComputePipeline.cpp
    ../cpp/rnwgpu/api/GPUCanvasContext.cpp
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

        SizeHolder/SizeHolder.h
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

#find_library("", react_codegen_RNWgpuViewSpec)

#set(BUILD_DIR "${CMAKE_SOURCE_DIR}/build")
#file(GLOB SO_DIR "/Users/piaskowyk/project/react-native-webgpu/package/example/android/app/build/intermediates/stripped_native_libs/debug/out/lib/arm64-v8a/")
#MESSAGE(WARNING "${CMAKE_SOURCE_DIR}")
#MESSAGE(WARNING "${BUILD_DIR}/intermediates/stripped_native_libs/*/${ANDROID_ABI}")
#find_library(
#        WGPU_LIB
#        react_codegen_RNWgpuViewSpec
#        PATHS ${SO_DIR}
#        NO_DEFAULT_PATH
#        NO_CMAKE_FIND_ROOT_PATH
#)

message(WARNING "dzik2: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message(WARNING "dzik3: ${CMAKE_PREFIX_PATH} | ${CMAKE_FRAMEWORK_PATH} | ${CMAKE_APPBUNDLE_PATH}}")
#message(WARNING "dzik4: ${ReactAndroid_FOUND}, ${ReactAndroid_INCLUDE_DIR}, ${ReactAndroid_INCLUDE_DIRS} or ${ReactAndroid_LIBRARIES}")
message(WARNING "dzik4: ${ReactAndroid_FOUND}, ${ReactAndroid_DIR}, ${ReactAndroid_CONFIG_DIR}")
#message(WARNING "Shared libraries are in these directories: $ENV{LD_LIBRARY_PATH}")
#file(GLOB SO_DIR "/Users/piaskowyk/project/react-native-webgpu/package/example/android/app/build/intermediates/cxx/Debug/1k334u16/obj/arm64-v8a/*")
#find_library(MyStaticFieldVAR MyStaticField HINTS "/Users/piaskowyk/project/react-native-webgpu/package/example/android/app/build/intermediates/cxx/Debug/1k334u16/obj/arm64-v8a/*")

#find_package(MyStaticField REQUIRED CONFIG)

# Include the path to MyClass.h
#include_directories("../SizeHolder")

# Link the MyStaticField library
#add_subdirectory("../SizeHolder" "../SizeHolder")

# Include the path to MyClass.h
include_directories("./SizeHolder")

# Link the MyStaticField library
add_subdirectory("./SizeHolder" "./SizeHolder")

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
#        ${MyStaticFieldVAR}
        MyStaticField
#        WGPU_LIB
    )
