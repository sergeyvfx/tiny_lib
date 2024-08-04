# Copyright (c) 2021 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

# Utilities to manipulate compilation flags.

################################################################################
# Append flag to an existing set of flags avoiding redundant spaces.
#
# Example of use is to append flags to CFLAGS and LINKER_FLAGS.
#
# Example:
#
#   set(MY_FLAGS)
#   add_compiler_flag(MY_FLAGS "-Wall")    # MY_FLAGS = "-Wall"
#   add_compiler_flag(MY_FLAGS "-Werror")  # MY_FLAGS = "-Wall -Werror"
#
# It is possible to append multiple flags:
#
#   add_compiler_flag(MY_FLAGS "-Wall" "-Werror")
#   add_compiler_flag(MY_FLAGS -Wall -Werror)
#
#   set(new_flags "-Wall" "-Werror")
#   add_compiler_flag(MY_FLAGS ${new_flags})
#
#   set(new_flags -Wall -Werror)
#   add_compiler_flag(MY_FLAGS ${new_flags})

macro(add_compiler_flag VARIABLE_NAME)
  foreach(_flag ${ARGN})
    if(NOT "${${VARIABLE_NAME}}" STREQUAL "")
      set(${VARIABLE_NAME} "${${VARIABLE_NAME}} ${_flag}")
    else()
      set(${VARIABLE_NAME} "${_flag}")
    endif()
  endforeach()
endmacro()

################################################################################
# Test whether flag is supported by compiler and if so add it to a variable.
#
# The separate C and C++ language variants exists.
#
# Example:
#
#   add_cxx_flag_if_supported(CXX_CANCEL_STRICT_FLAGS -Wno-class-memaccess)
#
#   If the compiler supports `-Wno-class-memaccess` it will be appended to the
#   end of the current content of the `CXX_CANCEL_STRICT_FLAGS` variable.

macro(_add_compiler_flag_if_supported_impl
    FLAGS_VAR
    FLAG_TO_ADD_IF_SUPPORTED
    LANG)
  # Use of whitespace or '-' in variable names (used by CheckCSourceCompiles
  # as #defines) will trigger errors.
  string(STRIP "${FLAG_TO_ADD_IF_SUPPORTED}" FLAG_TO_ADD_IF_SUPPORTED)

  # Build an informatively named test result variable so that it will be evident
  # which tests were performed/succeeded in the CMake output, e.g for -Wall:
  #
  # -- Performing Test CHECK_C_FLAG_Wall - Success
  #
  # NOTE: This variable is also used to cache test result.
  string(REGEX REPLACE "[-=]" "_" CHECK_${LANG}_FLAG
         "CHECK_${LANG}_FLAG${FLAG_TO_ADD_IF_SUPPORTED}")

  if("${LANG}" STREQUAL "C")
    include(CheckCCompilerFlag)
    check_c_compiler_flag(${FLAG_TO_ADD_IF_SUPPORTED} ${CHECK_${LANG}_FLAG})
  elseif("${LANG}" STREQUAL "CXX")
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(${FLAG_TO_ADD_IF_SUPPORTED} ${CHECK_${LANG}_FLAG})
  else()
    message(FATAL_ERROR "Unknown language ${LANG}")
  endif()

  if(${CHECK_${LANG}_FLAG})
    add_compiler_flag(${FLAGS_VAR} ${FLAG_TO_ADD_IF_SUPPORTED})
  endif()

  unset(CHECK_${LANG}_FLAG)
endmacro()

macro(add_c_flag_if_supported
    FLAGS_VAR
    FLAG_TO_ADD_IF_SUPPORTED)
  _add_compiler_flag_if_supported_impl(
      "${FLAGS_VAR}" "${FLAG_TO_ADD_IF_SUPPORTED}" "C")
endmacro()

macro(add_cxx_flag_if_supported
    FLAGS_VAR
    FLAG_TO_ADD_IF_SUPPORTED)
  _add_compiler_flag_if_supported_impl(
      "${FLAGS_VAR}" "${FLAG_TO_ADD_IF_SUPPORTED}" "CXX")
endmacro()

################################################################################
# Replace one compilation flag with another.
#
# For example:
#
#   set(CMAKE_C_FLAGS "--specs=nosys.specs -mcpu=cortex-m7")
#   replace_compiler_flag(CMAKE_C_FLAGS
#                         "--specs=nosys.specs" "--specs=nano.specs")
#
# will produce CMAKE_C_FLAGS = "--specs=nano.specs -mcpu=cortex-m7"

macro(replace_compiler_flag FLAGS_VAR OLD_FLAG NEW_FLAG)
  string(REPLACE "${OLD_FLAG}" "${NEW_FLAG}" ${FLAGS_VAR} "${${FLAGS_VAR}}")
endmacro()

################################################################################
# Remove compilation flags from given variable.
#
# For example:
#
#    set(CMAKE_C_FLAGS "-Wall -Werror -Wpedantic")
#    remove_compiler_flags(CMAKE_C_FLAGS "-Werror")
#
# will produce CMAKE_C_FLAGS = "-Wall -Wpedantic"

macro(remove_compiler_flags FLAGS_VAR)
  foreach(_flag "${ARGN}")
    string(REPLACE "${_flag}" "" ${FLAGS_VAR} "${${FLAGS_VAR}}")
  endforeach()
endmacro()

################################################################################
# Remove single or multiple compilation flag from all active languages and
# configurations.

macro(remove_all_active_compilers_flags)
  foreach(_lang C;CXX)
    remove_compiler_flags(CMAKE_${_lang}_FLAGS "${ARGN}")
    remove_compiler_flags(CMAKE_${_lang}_FLAGS_DEBUG "${ARGN}")
    remove_compiler_flags(CMAKE_${_lang}_FLAGS_RELEASE "${ARGN}")
    remove_compiler_flags(CMAKE_${_lang}_FLAGS_MINSIZEREL "${ARGN}")
  endforeach()
endmacro()

################################################################################
# Remove all strict flags which are desired for own codebase but which generates
# a lot of warnings (possible treated as errors) in third party source code.
#
# Affects currently configure CMAKE_<LANG>_FLAGS variables.

macro(remove_active_strict_compiler_flags)
  remove_all_active_compilers_flags(${REMOVE_CC_STRICT_FLAGS})

  add_compiler_flag(CMAKE_C_FLAGS ${CANCEL_C_STRICT_FLAGS})
  add_compiler_flag(CMAKE_C_FLAGS ${CANCEL_CC_STRICT_FLAGS})
  add_compiler_flag(CMAKE_CXX_FLAGS ${CANCEL_CC_STRICT_FLAGS})
  add_compiler_flag(CMAKE_CXX_FLAGS ${CANCEL_CXX_STRICT_FLAGS})
endmacro()

################################################################################
# Remove all strict flags which are desired for own codebase but which generates
# a lot of warnings (possible treated as errors) in third party source code.
#
# Affects compiler flags of a given file.

function(remove_file_strict_compiler_flags FILE)
  get_source_file_property(_file_cflags "${FILE}" COMPILE_FLAGS)
  if(NOT _file_cflags)
    # Avoid UNKNOWN value when the file does not have any flags configured yet.
    set(_file_cflags)
  endif()
  remove_compiler_flags(_file_cflags ${REMOVE_CC_STRICT_FLAGS})

  get_source_file_property(_file_lang "${FILE}" LANGUAGE)

  if(_file_lang STREQUAL C)
    add_compiler_flag(_file_cflags ${CANCEL_C_STRICT_FLAGS})
    add_compiler_flag(_file_cflags ${CANCEL_CC_STRICT_FLAGS})
  elseif(_file_lang STREQUAL CXX)
    add_compiler_flag(_file_cflags ${CANCEL_CC_STRICT_FLAGS})
    add_compiler_flag(_file_cflags ${CANCEL_CXX_STRICT_FLAGS})
  endif()

  set_source_files_properties(
      ${FILE} PROPERTIES COMPILE_FLAGS "${_file_cflags}")
endfunction()

################################################################################
# Disable exceptions handling for the specified target.

function(disable_target_exceptions TARGET)
  if (MSVC)
    # TODO: Needs implementation
    # target_compile_options(tl_${TEST_NAME}_noexc_test PRIVATE /EHsc-)
  else()
    target_compile_options(tl_${TEST_NAME}_noexc_test PRIVATE -fno-exceptions)
  endif()
endfunction()
