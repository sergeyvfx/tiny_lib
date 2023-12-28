# Copyright (c) 2023 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

################################################################################
# Library.

set(PUBLIC_HEADERS
  tl_temp_dir.h
  tl_temp_file.h
)

add_library(tl_temp INTERFACE ${PUBLIC_HEADERS})

################################################################################
# Regression tests.

tl_test(temp_dir
        test/tl_temp_dir_test.cc
        LIBRARIES tl_temp)

tl_test(temp_file
        test/tl_temp_file_test.cc
        LIBRARIES tl_temp)
