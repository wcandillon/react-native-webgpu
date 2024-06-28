cmake_minimum_required(VERSION 3.22.1)
cmake_policy(SET CMP0048 NEW)
project(webgpu VERSION 1.1.0 LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/BundleLibraries.cmake)

set(DAWN_BUILD_ANDROID_SAMPLES OFF)
set(DAWN_BUILD_SAMPLES OFF)
set(TINT_BUILD_TESTS OFF)
set(TINT_BUILD_CMD_TOOLS OFF)
set(TINT_BUILD_IR_BINARY OFF)

add_subdirectory(../../../externals/dawn/ dawn)

bundle_libraries(webgpu_c_bundled webgpu_dawn)

target_compile_definitions(webgpu_dawn
PRIVATE
    "WGPU_IMPLEMENTATION"
    "DAWN_NATIVE_IMPLEMENTATION"
    "DAWN_PLATFORM_IMPLEMENTATION"
    "DAWN_WIRE_IMPLEMENTATION")

target_compile_definitions(webgpu_dawn
PUBLIC
    "WGPU_SHARED_LIBRARY"
    "DAWN_NATIVE_SHARED_LIBRARY"
    "DAWN_PLATFORM_SHARED_LIBRARY"
    "DAWN_WIRE_SHARED_LIBRARY")

if(ANDROID)
  find_library(log-lib log)
  target_link_libraries(webgpu_c_bundled ${log-lib})
elseif(IOS)
  target_link_libraries(webgpu_c_bundled "-framework CoreFoundation" "-framework IOSurface" "-framework Metal" "-framework QuartzCore" "-framework Foundation")
endif()