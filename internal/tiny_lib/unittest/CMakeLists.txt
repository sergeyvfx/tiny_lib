# Copyright (c) 2021 tiny lib authors
#
# SPDX-License-Identifier: MIT-0

add_library(tl_test_main
  internal/test.cc
  internal/test_main.cc

  internal/test-internal.h

  mock.h
  test.h
)

target_link_libraries(tl_test_main
  GTest::gtest
  GTest::gmock
  gflags::gflags
)

tl_test(unittest internal/unittest_test.cc)

