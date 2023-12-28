# Copyright (c) 2022 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

################################################################################
# Library.

set(PUBLIC_HEADERS
  tl_log.h
)

add_library(tl_log INTERFACE ${PUBLIC_HEADERS})

################################################################################
# Regression tests.

tl_test(log
        test/tl_log_test.cc
        LIBRARIES tl_log)
