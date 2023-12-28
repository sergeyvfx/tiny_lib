# Copyright (c) 2022, tiny-libs authors
#
# SPDX-License-Identifier: MIT-0

# Remove strict flags from code which we don't maintain and can not fix warnings
# anyway.
remove_active_strict_compiler_flags()

if(WITH_TESTS)
  add_subdirectory(gflags)
  add_subdirectory(googletest)
endif()
