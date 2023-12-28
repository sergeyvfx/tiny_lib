# Copyright (c) 2022 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

################################################################################
# Library.

set(PUBLIC_HEADERS
  tl_result.h
)

add_library(tl_result INTERFACE ${PUBLIC_HEADERS})

################################################################################
# Regression tests.

tl_test(result
        test/tl_result_test.cc
        LIBRARIES tl_result)
