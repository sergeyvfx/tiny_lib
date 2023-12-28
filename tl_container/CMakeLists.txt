# Copyright (c) 2022 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

################################################################################
# Library.

set(PUBLIC_HEADERS
  tl_static_vector.h
)

add_library(tl_container INTERFACE ${PUBLIC_HEADERS})

################################################################################
# Regression tests.

tl_test(static_vector
        test/tl_static_vector_test.cc
        LIBRARIES tl_container)
