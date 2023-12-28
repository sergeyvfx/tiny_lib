# Copyright (c) 2022 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

################################################################################
# Library.

set(PUBLIC_HEADERS
  tl_cstring_view.h
  tl_static_string.h
  tl_string_portable.h
)

add_library(tl_string INTERFACE ${PUBLIC_HEADERS})

################################################################################
# Regression tests.

tl_test(cstring_view
        test/tl_cstring_view_test.cc
        LIBRARIES tl_string)

tl_test(static_string
        test/tl_static_string_test.cc
        LIBRARIES tl_string)

tl_test(string_portable
        test/tl_string_portable_test.cc
        LIBRARIES tl_string)
