// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#pragma once

#include "gtest/internal/gtest-internal.h"

namespace testing::internal {

// Specialize IsContainerTest in a way that std::span is considered a container.
//
// The Google's IsContainerTest() does not recognize it as a container because
// the span does not define the const_iterator type.
//
// Need to do it before including the gtest.h because otherwise the universal
// printer will not use this specialization.
template <class C,
          class Iterator = decltype(::std::declval<const C&>().begin()),
          class = decltype(::std::declval<const C&>().end()),
          class = decltype(++::std::declval<Iterator&>()),
          class = decltype(*::std::declval<Iterator>()),
          class = typename C::iterator,
          std::size_t Extent = C::extent>
auto IsContainerTest(int /* dummy */) -> IsContainer {
  return 0;
}

// Helper function for implementing ASSERT_NEAR.
//
// Similar to the one provided by the GTest itself, but avoids double promotion
// warning generated whenever ASSERT_NEAR/EXPECT_NEAR is used on a single
// floating point precision values.
//
// INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
GTEST_API_ auto DoubleNearPredFormat(const char* expr1,
                                     const char* expr2,
                                     const char* abs_error_expr,
                                     float val1,
                                     float val2,
                                     float abs_error)
    -> ::testing::AssertionResult;

}  // namespace testing::internal
