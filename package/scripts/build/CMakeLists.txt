cmake_minimum_required(VERSION 3.22.1)
cmake_policy(SET CMP0048 NEW)
project(webgpu VERSION 1.1.0 LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/BundleLibraries.cmake)

set(DAWN_BUILD_ANDROID_SAMPLES OFF)
set(DAWN_BUILD_SAMPLES OFF)
set(TINT_BUILD_TESTS OFF)
set(TINT_BUILD_CMD_TOOLS OFF)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

add_subdirectory(../../../../../external/dawn/ dawn)


add_library(native_dawn main.cpp)

target_link_libraries(native_dawn webgpu_dawn)

bundle_libraries(webgpu_c_bundled native_dawn)


# target_link_libraries(webgpu_c_bundled ${log-lib} android)

# add_custom_command(DEPENDS webgpu_dawn POST_BUILD
#                    OUTPUT ${CMAKE_SOURCE_DIR}/gen/include/dawn/webgpu.h
#                    COMMAND rm -r "${CMAKE_SOURCE_DIR}/gen" || echo
#                    COMMAND ${CMAKE_COMMAND} -E copy
#                            ${CMAKE_CURRENT_BINARY_DIR}/dawn/gen/include/dawn/webgpu.h
#                            ${CMAKE_SOURCE_DIR}/gen/include/dawn/webgpu.h)