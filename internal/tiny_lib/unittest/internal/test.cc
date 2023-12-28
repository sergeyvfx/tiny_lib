// Copyright (c) 2021 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#include "tiny_lib/unittest/test.h"

#include <cmath>

#include "gflags/gflags.h"

DECLARE_string(test_srcdir);

namespace testing::internal {

GTEST_API_ auto DoubleNearPredFormat(const char* expr1,
                                     const char* expr2,
                                     const char* abs_error_expr,
                                     const float val1,
                                     const float val2,
                                     const float abs_error)
    -> ::testing::AssertionResult {
  const float diff = fabsf(val1 - val2);
  if (diff <= abs_error) {
    return AssertionSuccess();
  }

  // TODO(wan): do not print the value of an expression if it's
  // already a literal.
  return AssertionFailure()
         << "The difference between " << expr1 << " and " << expr2 << " is "
         << diff << ", which exceeds " << abs_error_expr << ", where\n"
         << expr1 << " evaluates to " << val1 << ",\n"
         << expr2 << " evaluates to " << val2 << ", and\n"
         << abs_error_expr << " evaluates to " << abs_error << ".";
}

}  // namespace testing::internal

namespace tiny_lib::testing {

auto TestFileAbsolutePath(const std::filesystem::path& filename)
    -> std::filesystem::path {
  return FLAGS_test_srcdir / filename;
}

}  // namespace tiny_lib::testing
