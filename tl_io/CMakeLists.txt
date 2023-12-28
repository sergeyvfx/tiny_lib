# Copyright (c) 2022 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

################################################################################
# Library.

set(PUBLIC_HEADERS
  tl_io_file.h
)

add_library(tl_io INTERFACE ${PUBLIC_HEADERS})

################################################################################
# Regression tests.

function(tl_io_test PRIMITIVE_NAME)
  tl_test(io_${PRIMITIVE_NAME}
          test/tl_io_${PRIMITIVE_NAME}_test.cc
          LIBRARIES tl_io
          ARGUMENTS --test_srcdir ${CMAKE_CURRENT_SOURCE_DIR}/test/data)
endfunction()

tl_io_test(file)
