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

find_package(ReactAndroid REQUIRED CONFIG)
find_package(fbjni REQUIRED CONFIG)

add_library(${PACKAGE_NAME} SHARED
    ./cpp/cpp-adapter.cpp
    ./cpp/RNFAndroidLogger.cpp
    ../cpp/rnwgpu/RNWebGPUManager.cpp
    ../cpp/rnwgpu/api/GPU.cpp
    ../cpp/rnwgpu/api/GPUAdapter.cpp
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
        #"${NODE_MODULES_DIR}/react-native/ReactAndroid/src/main/jni/react/turbomodule"
        "${NODE_MODULES_DIR}/react-native/ReactAndroid/src/main/java/com/facebook/react/turbomodule/core/jni"

        ../cpp
        ../cpp/rnwgpu
        ../cpp/rnwgpu/api
        ../cpp/jsi
        ../cpp/dawn
        ../cpp/webgpu

        ${libfbjni_include_DIRS}
)

# Link
target_link_libraries(
        ${PACKAGE_NAME}
        ${FBJNI_LIBRARY}
        log
        jnigraphics
        android
        fbjni::fbjni
        ReactAndroid::jsi
    )
