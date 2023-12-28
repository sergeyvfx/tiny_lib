// Copyright (c) 2023 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#include "tl_temp/tl_temp_file.h"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "tiny_lib/unittest/test.h"

namespace tiny_lib::temp_file {

TEST(tl_temp_file, Basic) {
  {
    std::filesystem::path path;

    {
      TempFile temp_file;

      ASSERT_TRUE(temp_file.Open("prefix", ".txt"));

      path = temp_file.GetFilePath();
      const std::string filename = path.filename().generic_string();

      EXPECT_TRUE(std::filesystem::exists(path));
      EXPECT_TRUE(filename.starts_with("prefix"));
      EXPECT_TRUE(filename.ends_with(".txt"));

      EXPECT_NE(temp_file.GetStream(), nullptr);
    }

    EXPECT_FALSE(std::filesystem::exists(path));
  }
}

// Test for read and write of a big buffer (over 4 GiB).
//
// Uses a lot of RAM and disk space, so is not enabled by default, but it is
// important to keep it tested on a bigger changes of the file implementation.
#if 0
TEST(tl_temp_file, Big) {
  TempFile temp_file;

  ASSERT_TRUE(temp_file.Open());

  FILE* stream = temp_file.GetStream();
  EXPECT_NE(stream, nullptr);

  {
    std::vector<uint8_t> data(size_t(5) * 1024 * 1024 * 1024 + 1);
    data.back() = 'X';

    EXPECT_EQ(::fwrite(data.data(), 1, data.size(), stream), data.size());
  }

  {
    std::vector<uint8_t> data(size_t(5) * 1024 * 1024 * 1024 + 1);

    ::fseek(stream, 0, SEEK_SET);
    EXPECT_EQ(::fread(data.data(), 1, data.size(), stream), data.size());
    EXPECT_EQ(data.back(), 'X');
  }
}
#endif

}  // namespace tiny_lib::temp_file
