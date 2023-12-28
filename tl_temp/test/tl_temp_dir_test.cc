// Copyright (c) 2023 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#include "tl_temp/tl_temp_dir.h"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

#include "tiny_lib/unittest/test.h"

namespace tiny_lib::temp_dir {

TEST(tl_temp_dir, Basic) {
  {
    std::filesystem::path path;

    {
      TempDir temp_dir;

      ASSERT_TRUE(temp_dir.Open("prefix", ".dir"));

      path = temp_dir.GetPath();
      const std::string filename = path.filename().generic_string();

      EXPECT_TRUE(std::filesystem::exists(path));
      EXPECT_TRUE(filename.starts_with("prefix"));
      EXPECT_TRUE(filename.ends_with(".dir"));
    }

    EXPECT_FALSE(std::filesystem::exists(path));
  }
}

}  // namespace tiny_lib::temp_dir
