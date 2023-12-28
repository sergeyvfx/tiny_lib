# Copyright (c) 2022 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

################################################################################
# Policies.

cmake_minimum_required(VERSION 3.19)

################################################################################
# Options.

option(WITH_TESTS "Build the unit tests" ON)

# Development options.
# Recommended for use by all developers.
option(WITH_DEVELOPER_STRICT
       "Use extra strict compilation flags such as -Werror" OFF)
option(WITH_DEVELOPER_SANITIZER
       "Use address sanitizer to validate code behavior" OFF)
option(WITH_DEVELOPER_CLANG_TIDY "Enable Clang-Tidy" OFF)

################################################################################
# CMake module path configuration and global includes.

list(APPEND CMAKE_MODULE_PATH
  ${CMAKE_SOURCE_DIR}/cmake
  ${CMAKE_SOURCE_DIR}/cmake/module
)

################################################################################
# Project initialization and configuration.

include(build_initialize)

project(tiny_lib C CXX)

include(build_configure)

################################################################################
# External libraries.

add_subdirectory(external)

################################################################################
# Global settings.
# Applicable for all targets defined in the subdirectories.
#
# NOTE: Ideally this is the only place which modifies global configuration.
# For the specific targets prefer to use `target_include_directories` and
# `target_compile_definitions`.

include_directories(
  .
  internal
)

################################################################################
# Source code folders.

if(WITH_DEVELOPER_CLANG_TIDY)
  find_package(ClangTidy REQUIRED)
  set(CMAKE_C_CLANG_TIDY ${ClangTidy_EXECUTABLE})
  set(CMAKE_CXX_CLANG_TIDY ${ClangTidy_EXECUTABLE})
endif()

add_subdirectory(internal)

add_subdirectory(tl_audio_wav)
add_subdirectory(tl_build_config)
add_subdirectory(tl_container)
add_subdirectory(tl_convert)
add_subdirectory(tl_functional)
add_subdirectory(tl_image_bmp)
add_subdirectory(tl_io)
add_subdirectory(tl_log)
add_subdirectory(tl_result)
add_subdirectory(tl_string)
add_subdirectory(tl_temp)
