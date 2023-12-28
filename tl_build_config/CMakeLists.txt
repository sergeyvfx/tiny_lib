# Copyright (c) 2022 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

################################################################################
# Library.

set(PUBLIC_HEADERS
  tl_build_config.h
)

add_library(tl_build_config INTERFACE ${PUBLIC_HEADERS})

################################################################################
# Regression tests.

tl_test(
    build_config test/tl_build_config_test.cc
    INCLUDES .
    LIBRARIES tl_build_config)
