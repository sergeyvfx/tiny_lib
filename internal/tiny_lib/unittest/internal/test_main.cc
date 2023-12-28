// Copyright (c) 2021 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#include "tiny_lib/unittest/test.h"

#include <gflags/gflags.h>

DEFINE_string(test_srcdir, "", "The location of data for this test.");

auto main(int argc, char** argv) -> int {
  testing::InitGoogleTest(&argc, argv);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  return RUN_ALL_TESTS();
}
