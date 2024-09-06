cmake_minimum_required(VERSION 3.13)
set(CMAKE_VERBOSE_MAKEFILE on)

file(GLOB react_codegen_SRCS CONFIGURE_DEPENDS *.cpp react/renderer/components/RNWgpuViewSpec/*.cpp)

add_library(
  react_codegen_RNWgpuViewSpec
  SHARED
  ${react_codegen_SRCS}
)

target_include_directories(react_codegen_RNWgpuViewSpec PUBLIC . react/renderer/components/RNWgpuViewSpec)

include_directories("../SizeHolder")
add_subdirectory("../SizeHolder" "../SizeHolder")

target_link_libraries(
  react_codegen_RNWgpuViewSpec
  fbjni
  folly_runtime
  glog
  jsi
  react_codegen_rncore
  react_debug
  react_nativemodule_core
  react_render_componentregistry
  react_render_core
  react_render_debug
  react_render_graphics
  react_render_imagemanager
  react_render_mapbuffer
  react_utils
  rrc_image
  rrc_view
  turbomodulejsijni
  yoga
  MyStaticField
)

target_compile_options(
  react_codegen_RNWgpuViewSpec
  PRIVATE
  -DLOG_TAG=\"ReactNative\"
  -fexceptions
  -frtti
  -std=c++20
  -Wall
)
