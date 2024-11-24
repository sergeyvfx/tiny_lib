// Copyright (c) 2021 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#include "tl_string/tl_string_portable.h"

#include <string_view>

#include "tiny_lib/unittest/test.h"

namespace tiny_lib::string_portable {

using std::string_view;

TEST(tl_string_portable, strncpy) {
  // NOLINTNEXTLINE(modernize-avoid-c-arrays)
  char buffer[64] = {0};

  {
    memset(buffer, 0, sizeof(buffer));
    EXPECT_EQ(strncpy(buffer, "", 1), 0);
    EXPECT_EQ(string_view(buffer), "");
  }

  {
    memset(buffer, 0, sizeof(buffer));
    EXPECT_EQ(strncpy(buffer, "foo", 1), 0);
    EXPECT_EQ(string_view(buffer), "");
  }

  {
    memset(buffer, 0, sizeof(buffer));
    EXPECT_EQ(strncpy(buffer, "foobar", 3), 2);
    EXPECT_EQ(string_view(buffer), "fo");
  }

  {
    // Test that no null-pointer is written then the buffer is advertized
    // as having zero size.
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = 'f';
    buffer[1] = 'o';
    EXPECT_EQ(strncpy(buffer, "foobar", 0), 0);
    EXPECT_EQ(string_view(buffer), "fo");
  }

  {
    memset(buffer, 0, sizeof(buffer));
    EXPECT_EQ(strncpy(buffer, "foobar", 16), 6);
    EXPECT_EQ(string_view(buffer), "foobar");
  }
}

TEST(tl_string_portable, snprintf) {
  {
    // NOLINTNEXTLINE(modernize-avoid-c-arrays)
    char buffer[8] = {0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f};

    // Small string, fits into the buffer.
    EXPECT_EQ(snprintf(buffer, sizeof(buffer), "%d", 10), 2);
    EXPECT_EQ(string_view(buffer), "10");
    EXPECT_EQ(buffer[2], '\0');
  }

  // Big destination, will clip.
  {
    // NOLINTNEXTLINE(modernize-avoid-c-arrays)
    char buffer[8] = {0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f};

    EXPECT_EQ(
        snprintf(buffer, sizeof(buffer), "%d%d%d%d%d", 12, 34, 56, 78, 90), 10);
    EXPECT_EQ(string_view(buffer), "1234567");
    EXPECT_EQ(buffer[7], '\0');
  }

  // Make sure no write past the given buffer size happens.
  {
    // NOLINTNEXTLINE(modernize-avoid-c-arrays)
    char buffer[9] = {0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f};

    EXPECT_EQ(
        snprintf(buffer, sizeof(buffer) - 1, "%d%d%d%d%d", 12, 34, 56, 78, 90),
        10);
    EXPECT_EQ(string_view(buffer), "1234567");
    EXPECT_EQ(buffer[7], '\0');
    EXPECT_EQ(buffer[8], 0x7f);
  }

  // Formatted print to a null string.
  {
    EXPECT_EQ(snprintf(nullptr, 0, "%d%d%d%d%d", 12, 34, 56, 78, 90), 10);
  }
}

}  // namespace tiny_lib::string_portable
