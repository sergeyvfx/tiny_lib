# Copyright (c) 2022 tiny lib authors
#
# SPDX-License-Identifier: MIT

#[=======================================================================[.rst:
FindClangTIdy
----------

Find cmake-tidy binary from Clang/LLVM project.

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables:

``ClangTidy_FOUND``
  True if the clang-tidy executable is found.

``ClangTidy_EXECUTABLE``
  A full path to an executable of clang-tidy.

``ClangTidy_VERSION``
  A semantic version of the clang-tidy, which only includes major, minor and
  patch versions.

``ClangTidy_VERSION_MAJOR``
  A major part of the clang-tidy version.

``ClangTidy_VERSION_MINTO``
  A minor part of the clang-tidy version.

`ClangTidy_VERSION_PATCH``
  A patch part of the clang-tidy version.
#]=======================================================================]

set(_ClangTidy_SEARCH_DIRS
  ${ClangTidy_ROOT_DIR}
  /usr/local/bin
)

# TODO(sergey): Find more future-proof way.
find_program(ClangTidy_EXECUTABLE
  NAMES
    clang-tidy-16
    clang-tidy-15
    clang-tidy-14
    clang-tidy-13
    clang-tidy-12
    clang-tidy
  HINTS
    ${_ClangTidy_SEARCH_DIRS}
)

if(ClangTidy_EXECUTABLE)
  # Setup fallback values.
  set(ClangTidy_VERSION_MAJOR 0)
  set(ClangTidy_VERSION_MINOR 0)
  set(ClangTidy_VERSION_PATCH 0)

  # Since the exact version might not be included into the executable file name
  # query it by running the command.
  execute_process(COMMAND ${ClangTidy_EXECUTABLE} -version
                  OUTPUT_VARIABLE _clang_tidy_version_string
                  ERROR_QUIET
                  OUTPUT_STRIP_TRAILING_WHITESPACE)

  # Parse parts.
  if(_clang_tidy_version_string MATCHES "LLVM version .*")
    # Strip the LLVM prefix.
    string(REGEX REPLACE
           ".*LLVM version ([.0-9]+).*" "\\1"
           _clang_tidy_semantic_version "${_clang_tidy_version_string}")

    # Get list of individual version components.
    string(REPLACE "." ";" _clang_tidy_version_parts
           "${_clang_tidy_semantic_version}")

    list(LENGTH _clang_tidy_version_parts _num_clang_tidy_version_parts)
    if(_num_clang_tidy_version_parts GREATER 0)
      list(GET _clang_tidy_version_parts 0 ClangTidy_VERSION_MAJOR)
    endif()
    if(_num_clang_tidy_version_parts GREATER 1)
      list(GET _clang_tidy_version_parts 1 ClangTidy_VERSION_MINOR)
    endif()
    if(_num_clang_tidy_version_parts GREATER 2)
      list(GET _clang_tidy_version_parts 2 ClangTidy_VERSION_PATCH)
    endif()

    # Unset temp variables.
    unset(_num_clang_tidy_version_parts)
    unset(_clang_tidy_semantic_version)
    unset(_clang_tidy_version_partss)
  else()
    # Make sure unknown nature of clang-tidy is recorded.
    set(_clang_tidy_version_string "UNKNOWN")
  endif()

  # Construct full semantic version.
  set(ClangTidy_VERSION "${ClangTidy_VERSION_MAJOR}.\
${ClangTidy_VERSION_MINOR}.\
${ClangTidy_VERSION_PATCH}")

  unset(_clang_tidy_version_string)
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ClangTidy
                                  REQUIRED_VARS ClangTidy_EXECUTABLE
                                  VERSION_VAR ClangTidy_VERSION)
