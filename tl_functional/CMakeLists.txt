# Copyright (c) 2022 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

################################################################################
# Library.

set(PUBLIC_HEADERS
  tl_callback.h
)

add_library(tl_functional INTERFACE ${PUBLIC_HEADERS})

################################################################################
# Regression tests.

tl_test(functional_callback
        test/tl_callback_test.cc
        LIBRARIES tl_functional)
