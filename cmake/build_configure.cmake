# Copyright (c) 2021 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

# Configure build system after the CMake initialized the project.
#
# This file is to be included after the `project()` has been called.

################################################################################
# Default build type.
#
# Based on https://www.kitware.com/cmake-and-the-default-build-type/

# Default to the release build type.
if(NOT CMAKE_CONFIGURATION_TYPES)
  if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to Release as none was specified.")

    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel ..."
        FORCE)
  endif()

  # Set the possible values of build type for cmake-gui.
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
               "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

################################################################################
# Directory configuration.

get_property(GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

set(EXECUTABLE_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin)
set(TEST_EXECUTABLE_OUTPUT_DIR ${EXECUTABLE_OUTPUT_DIR}/tests)
set(LIBRARY_OUTPUT_DIR ${CMAKE_BINARY_DIR}/lib)

if(GENERATOR_IS_MULTI_CONFIG AND NOT WIN32)
  set(EXECUTABLE_OUTPUT_DIR ${EXECUTABLE_OUTPUT_DIR}/$<CONFIG>)
  set(TEST_EXECUTABLE_OUTPUT_DIR ${TEST_EXECUTABLE_OUTPUT_DIR}/$<CONFIG>)
  set(LIBRARY_OUTPUT_DIR ${LIBRARY_OUTPUT_DIR}/$<CONFIG>)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_OUTPUT_DIR})

################################################################################
# CMake module dependencies.

include(compiler_flags)
include(target)

################################################################################
# External libraries.

include(external_libs)

################################################################################
# Unit testing.

enable_testing()

include(target_test)

################################################################################
# Platform specific configuration.

if(UNIX AND NOT APPLE)
  include(platform/platform_unix)
elseif(WIN32)
  include(platform/platform_win32)
elseif(APPLE)
  include(platform/platform_apple)
endif()
