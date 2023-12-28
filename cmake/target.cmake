# Copyright (c) 2021 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

# Generic utility function manipulating target properties.

# Set binary executable output directory.
#
# Deals with MSVC which uses separate folders for release and debug builds.
function(target_set_output_directory TARGET_NAME OUTPUT_DIRECTORY)
  if(MSVC)
    set(output_dir_release "${OUTPUT_DIRECTORY}/Release")
    set(output_dir_debug "${OUTPUT_DIRECTORY}/Debug")
  else()
    set(output_dir_release "${OUTPUT_DIRECTORY}")
    set(output_dir_debug "${OUTPUT_DIRECTORY}")
  endif()
  set_target_properties(
    "${TARGET_NAME}" PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${OUTPUT_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${output_dir_release}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${output_dir_debug}"
  )
endfunction()

# An utility function which configures IDE's debugger command line arguments for
# the given target.
function(target_set_debugger_command_arguments TARGET_NAME)
  set(escaped_arguments)
  foreach(argument ${ARGN})
    if(${argument} MATCHES "[ \t\r\n]")
      set(argument "\"${argument}\"")
    endif()
    list(APPEND escaped_arguments ${argument})
  endforeach()
  string(REPLACE ";" " " arguments_command_line "${escaped_arguments}")

  if(MSVC)
    if(${CMAKE_VERSION} VERSION_GREATER "3.12")
      set_target_properties(${TARGET_NAME} PROPERTIES
           VS_DEBUGGER_COMMAND_ARGUMENTS "${arguments_command_line}")
    endif()
  endif()
endfunction()
