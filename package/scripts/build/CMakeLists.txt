cmake_minimum_required(VERSION 3.22.1)
cmake_policy(SET CMP0048 NEW)
project(webgpu VERSION 1.1.0 LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/BundleLibraries.cmake)

set(DAWN_BUILD_ANDROID_SAMPLES OFF)
set(DAWN_BUILD_SAMPLES OFF)
set(TINT_BUILD_TESTS OFF)
set(TINT_BUILD_CMD_TOOLS OFF)

add_subdirectory(../../../externals/dawn/ dawn)

add_library(native_dawn main.cpp)

target_link_libraries(native_dawn webgpu_dawn)

bundle_libraries(webgpu_c_bundled native_dawn)

if(ANDROID)
  find_library(log-lib log)
  target_link_libraries(webgpu_c_bundled ${log-lib} android)
elseif(IOS)
  target_link_libraries(webgpu_c_bundled "-framework CoreFoundation" "-framework IOSurface" "-framework Metal" "-framework QuartzCore" "-framework Foundation")
endif()