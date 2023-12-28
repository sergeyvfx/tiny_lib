# Copyright (c) 2022 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

################################################################################
# Library.

set(PUBLIC_HEADERS
  tl_image_bmp_reader.h
  tl_image_bmp_writer.h
)

add_library(tl_image_bmp INTERFACE ${PUBLIC_HEADERS})

################################################################################
# Regression tests.

function(tl_image_bmp_test PRIMITIVE_NAME)
  tl_test(image_bmp_${PRIMITIVE_NAME}
          test/tl_image_bmp_${PRIMITIVE_NAME}_test.cc
          LIBRARIES tl_image_bmp)
endfunction()

tl_image_bmp_test(reader)
tl_image_bmp_test(writer)
