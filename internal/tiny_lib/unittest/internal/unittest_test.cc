// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#include "tiny_lib/unittest/test.h"

#include <array>
#include <span>
#include <sstream>

namespace tiny_lib::testing {

TEST(test, PrintSpan) {
  const std::array array = std::to_array<int>({1, 2, 3, 4});
  const std::span<const int> span{array};

  std::stringstream stream;
  testing::internal::PrintTo<std::span<const int>>(span, &stream);

  EXPECT_EQ(stream.str(), "{ 1, 2, 3, 4 }");
}

}  // namespace tiny_lib::testing
