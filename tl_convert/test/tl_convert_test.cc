// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT

#include "tl_convert/tl_convert.h"

#include <array>
#include <string>

#include "tiny_lib/unittest/test.h"

namespace tiny_lib::convert {

TEST(tl_convert, StringToInt) {
  { EXPECT_EQ(StringToInt<int>(""), 0); }

  {
    EXPECT_EQ(StringToInt<int>("0"), 0);
    EXPECT_EQ(StringToInt<int>("1"), 1);
    EXPECT_EQ(StringToInt<int>("13"), 13);

    EXPECT_EQ(StringToInt<int>("+13"), 13);
    EXPECT_EQ(StringToInt<int>("-13"), -13);

    EXPECT_EQ(StringToInt<int32_t>("-2147483648"), -2147483648);
    EXPECT_EQ(StringToInt<int32_t>("2147483647"), 2147483647);

    // Use -9223372036854775807-1 as the -9223372036854775808 are two literals
    // ("-" and "9223372036854775808") and the former one does not fit into a
    // signed integral value.
    EXPECT_EQ(StringToInt<int64_t>("-9223372036854775808"),
              -9223372036854775807 - 1);
    EXPECT_EQ(StringToInt<int64_t>("9223372036854775807"),
              9223372036854775807LL);
  }

  // Remainder.
  {
    std::string_view remainder;

    EXPECT_EQ(StringToInt<int>("0", remainder), 0);
    EXPECT_EQ(remainder, "");

    EXPECT_EQ(StringToInt<int>("18.45", remainder), 18);
    EXPECT_EQ(remainder, ".45");
  }

  // Leading whitespace.
  {
    EXPECT_EQ(StringToInt<int32_t>("  -2147483648"), -2147483648);

    std::string_view remainder;

    EXPECT_EQ(StringToInt<int>("  12  ", remainder), 12);
    EXPECT_EQ(remainder, "  ");

    EXPECT_EQ(StringToInt<int>("  ", remainder), 0);
    EXPECT_EQ(remainder, "  ");
  }
}

TEST(tl_convert, StringToFloat) {
  constexpr float kTolerance = 1e-6f;

  // Empty string.
  { EXPECT_EQ(StringToFloat<float>(""), 0); }

  // Integer values.
  {
    EXPECT_NEAR(StringToFloat<float>("0"), 0, kTolerance);
    EXPECT_NEAR(StringToFloat<float>("1"), 1, kTolerance);
    EXPECT_NEAR(StringToFloat<float>("13"), 13, kTolerance);

    EXPECT_NEAR(StringToFloat<float>("+13"), 13, kTolerance);
    EXPECT_NEAR(StringToFloat<float>("-13"), -13, kTolerance);
  }

  // Fractional values.
  {
    EXPECT_NEAR(StringToFloat<float>("0.123"), 0.123f, kTolerance);
    EXPECT_NEAR(StringToFloat<float>("1.2345"), 1.2345f, kTolerance);
    EXPECT_NEAR(StringToFloat<float>("13.2345"), 13.2345f, kTolerance);

    EXPECT_NEAR(StringToFloat<float>("+13.2345"), 13.2345f, kTolerance);
    EXPECT_NEAR(StringToFloat<float>("-13.2345"), -13.2345f, kTolerance);
  }

  // Corner cases
  {
    // Missing decimal part.
    EXPECT_NEAR(StringToFloat<float>(".00002182"), 0.00002182f, kTolerance);
    EXPECT_NEAR(StringToFloat<float>("-.00002182"), -0.00002182f, kTolerance);
  }

  // Remainder.
  {
    std::string_view remainder;

    EXPECT_EQ(StringToFloat<float>("0", remainder), 0);
    EXPECT_EQ(remainder, "");

    EXPECT_EQ(StringToFloat<int>("18,45", remainder), 18);
    EXPECT_EQ(remainder, ",45");
  }

  // Leading whitespace.
  {
    EXPECT_NEAR(StringToFloat<float>("  +13.2345"), 13.2345f, kTolerance);

    std::string_view remainder;

    EXPECT_EQ(StringToFloat<float>("  12  ", remainder), 12);
    EXPECT_EQ(remainder, "  ");

    EXPECT_EQ(StringToFloat<float>("  ", remainder), 0);
    EXPECT_EQ(remainder, "  ");
  }
}

TEST(tl_convert, IntToStringBuffer) {
  // Typical use-cases.
  {
    {
      std::array<char, 32> buffer;
      std::fill(buffer.begin(), buffer.end(), 'X');

      EXPECT_TRUE(IntToStringBuffer(0, buffer));
      EXPECT_STREQ(buffer.data(), "0");
    }

    {
      std::array<char, 32> buffer;
      std::fill(buffer.begin(), buffer.end(), 'X');

      EXPECT_TRUE(IntToStringBuffer(12345, buffer));
      EXPECT_STREQ(buffer.data(), "12345");
    }

    {
      std::array<char, 32> buffer;
      std::fill(buffer.begin(), buffer.end(), 'X');

      EXPECT_TRUE(IntToStringBuffer(-12345, buffer));
      EXPECT_STREQ(buffer.data(), "-12345");
    }
  }

  // Check for boundaries and overflows.
  {
    // Just enough space for actual data and the null-terminator.
    {
      std::array<char, 4> buffer;
      EXPECT_TRUE(IntToStringBuffer(123, buffer));
      EXPECT_STREQ(buffer.data(), "123");
    }
    {
      std::array<char, 4> buffer;
      EXPECT_TRUE(IntToStringBuffer(-12, buffer));
      EXPECT_STREQ(buffer.data(), "-12");
    }

    // Not enough space for null-terminator.
    {
      std::array<char, 4> buffer;
      std::fill(buffer.begin(), buffer.end(), 'X');

      std::span<char> span(buffer.data(), 3);
      EXPECT_FALSE(IntToStringBuffer(123, span));
      EXPECT_EQ(buffer[3], 'X');
    }
    {
      std::array<char, 4> buffer;
      std::fill(buffer.begin(), buffer.end(), 'X');

      std::span<char> span(buffer.data(), 3);
      EXPECT_FALSE(IntToStringBuffer(-12, span));
      EXPECT_EQ(buffer[3], 'X');
    }

    // Not enough space for data.
    {
      std::array<char, 4> buffer;
      std::fill(buffer.begin(), buffer.end(), 'X');

      std::span<char> span(buffer.data(), 3);
      EXPECT_FALSE(IntToStringBuffer(1234, span));
      EXPECT_EQ(buffer[3], 'X');
    }
    {
      std::array<char, 4> buffer;
      std::fill(buffer.begin(), buffer.end(), 'X');

      std::span<char> span(buffer.data(), 3);
      EXPECT_FALSE(IntToStringBuffer(-123, span));
      EXPECT_EQ(buffer[3], 'X');
    }
  }
}

}  // namespace tiny_lib::convert
