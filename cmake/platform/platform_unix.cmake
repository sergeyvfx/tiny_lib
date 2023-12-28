# Copyright (c) 2018 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

# Platform-specific configuration for all non-Apple UNIX platforms.

include(platform/platform_unix_common)

set(DEFAULT_C_FLAGS)
set(DEFAULT_CXX_FLAGS)
set(DEFAULT_LINKER_FLAGS)

################################################################################
# Linker and compiler flags tweaks.

add_compiler_flag(CMAKE_C_FLAGS "${DEFAULT_C_FLAGS}")
add_compiler_flag(CMAKE_CXX_FLAGS "${DEFAULT_CXX_FLAGS}")

add_compiler_flag(CMAKE_EXE_LINKER_FLAGS "${DEFAULT_LINKER_FLAGS}")
add_compiler_flag(CMAKE_SHARED_LINKER_FLAGS "${DEFAULT_LINKER_FLAGS}")
add_compiler_flag(CMAKE_MODULE_LINKER_FLAGS "${DEFAULT_LINKER_FLAGS}")

unset(DEFAULT_C_FLAGS)
unset(DEFAULT_CXX_FLAGS)
unset(DEFAULT_LINKER_FLAGS)
