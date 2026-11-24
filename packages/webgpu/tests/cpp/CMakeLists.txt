cmake_minimum_required(VERSION 3.24)
project(react_native_webgpu_cpp_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Use the same prepared headers shipped to CocoaPods and SwiftPM consumers.
# Unit tests need no Dawn library and perform no binary download here.
set(DAWN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../cpp/dawn")
if(NOT EXISTS "${DAWN_DIR}/include/webgpu/webgpu.h")
  message(FATAL_ERROR "react-native-webgpu: Dawn headers are missing. Run yarn copy-dawn-headers first.")
endif()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/releases/download/v1.18.0/googletest-1.18.0.tar.gz
  URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
  DOWNLOAD_EXTRACT_TIMESTAMP True
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(
  cpp-tests
  DawnBlobCacheTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp/rnwgpu/DawnBlobCache.cpp
)
target_include_directories(
  cpp-tests
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp/rnwgpu
  ${DAWN_DIR}/include
)
target_compile_options(cpp-tests PRIVATE -Wall -Wextra)
target_link_libraries(cpp-tests PRIVATE GTest::gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(cpp-tests)
