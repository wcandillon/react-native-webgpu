# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

project(MyStaticFieldProject)

# Add the source files
add_library(MyStaticField SHARED
        SizeHolder.cpp
)

target_include_directories(MyStaticField PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(
        MyStaticField
        PROPERTIES
        LINKER_LANGUAGE
        CXX
)

# Export the library targets
export(TARGETS MyStaticField FILE MyStaticFieldConfig.cmake)

#install(TARGETS MyStaticField DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
#install(FILES SizeHolder.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

#install(TARGETS MyStaticField
#        ARCHIVE DESTINATION lib
#)
#
#install(FILES SizeHolder.h
#        DESTINATION include
#)
