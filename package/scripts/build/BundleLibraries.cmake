function(bundle_libraries output_target)
  set(options)
  set(oneValueArgs LIBRARY_TYPE)
  set(multiValueArgs TARGETS)
  cmake_parse_arguments(bundle_libraries "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  function(get_dependencies input_target)
    get_target_property(alias ${input_target} ALIASED_TARGET)
    if(TARGET ${alias})
      set(input_target ${alias})
    endif()
    if(${input_target} IN_LIST all_dependencies)
      return()
    endif()
    list(APPEND all_dependencies ${input_target})

    get_target_property(link_libraries ${input_target} LINK_LIBRARIES)
    foreach(dependency IN LISTS link_libraries)
      if(TARGET ${dependency})
        get_dependencies(${dependency})
      endif()
    endforeach()

    get_target_property(link_libraries ${input_target} INTERFACE_LINK_LIBRARIES)
    foreach(dependency IN LISTS link_libraries)
      if(TARGET ${dependency})
        get_dependencies(${dependency})
      endif()
    endforeach()

    set(all_dependencies ${all_dependencies} PARENT_SCOPE)
  endfunction()

  foreach(input_target IN LISTS bundle_libraries_TARGETS)
    get_dependencies(${input_target})
  endforeach()

  foreach(dependency IN LISTS all_dependencies)
    get_target_property(type ${dependency} TYPE)
    if(${type} STREQUAL "STATIC_LIBRARY")
      list(APPEND all_objects $<TARGET_OBJECTS:${dependency}>)
    endif()
  endforeach()

  add_library(${output_target} ${bundle_libraries_LIBRARY_TYPE} ${all_objects})

  add_dependencies(${output_target} ${bundle_libraries_TARGETS})

endfunction()