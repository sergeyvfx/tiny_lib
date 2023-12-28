# Copyright (c) 2022 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

################################################################################
# Library.

set(PUBLIC_HEADERS
  tl_convert.h
)

add_library(tl_convert INTERFACE ${PUBLIC_HEADERS})

################################################################################
# Regression tests.

tl_test(convert
        test/tl_convert_test.cc
        LIBRARIES tl_convert)
