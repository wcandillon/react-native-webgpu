cmake_minimum_required(VERSION 3.22)
project(react_native_webgpu_cpp_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Dawn's headers come from the react-native-webgpu-dawn package, as in the
# Android and iOS builds. The tests link FakeWebGPU.cpp instead of Dawn.
execute_process(
  COMMAND node -e "process.stdout.write(require('path').dirname(require.resolve('react-native-webgpu-dawn/package.json')))"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE DAWN_DIR
  RESULT_VARIABLE DAWN_DIR_RESULT
)
if(NOT DAWN_DIR_RESULT EQUAL 0)
  message(FATAL_ERROR "react-native-webgpu: the react-native-webgpu-dawn package was not found. Install dependencies (yarn install) first.")
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
  AnimationCadenceTest.cpp
  FakeWebGPU.cpp
  SurfaceInfoTest.cpp
  SurfaceRegistryTest.cpp
)
target_include_directories(
  cpp-tests
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp/rnwgpu
  ${CMAKE_CURRENT_SOURCE_DIR}/../../android/cpp
  ${DAWN_DIR}/include
)
target_compile_options(cpp-tests PRIVATE -Wall -Wextra)
target_link_libraries(cpp-tests PRIVATE GTest::gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(cpp-tests)
