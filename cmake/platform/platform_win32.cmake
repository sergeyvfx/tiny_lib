# Copyright (c) 2018 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

# Platform-specific configuration for WIN32 platform.

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release)

if(MSVC)
  include(platform/platform_win32_msvc)
endif()
