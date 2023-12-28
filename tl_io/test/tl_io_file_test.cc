// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#include "tl_io/tl_io_file.h"

#include <array>
#include <span>
#include <string_view>
#include <vector>

#include <gflags/gflags.h>

#include "tiny_lib/unittest/mock.h"
#include "tiny_lib/unittest/test.h"

DECLARE_string(test_srcdir);

namespace tiny_lib::io_file {

using testing::Eq;
using testing::Pointwise;

using Path = std::filesystem::path;

constexpr std::string_view kASCIIFileName{"file.txt"};

// Non-ascii string 要らない from an example:
//   https://en.cppreference.com/w/cpp/filesystem/path/u8path
constexpr std::string_view kUnicodeFileName{"要らない.txt"};
constexpr std::u8string_view kU8UnicodeFileName{u8"要らない.txt"};

TEST(tl_io_file, Open) {
  {
    File file;
    EXPECT_TRUE(
        file.Open(Path{FLAGS_test_srcdir} / kASCIIFileName, File::kRead));
  }

  {
    File file;
    EXPECT_TRUE(file.Open(Path{FLAGS_test_srcdir} / Path(kU8UnicodeFileName),
                          File::kRead));
  }

  {
    File file;
    EXPECT_TRUE(
        file.Open(Path{FLAGS_test_srcdir} / kUnicodeFileName, File::kRead));
  }
}

TEST(tl_io_file, Size) {
  File file;

  file.Open(Path{FLAGS_test_srcdir} / kASCIIFileName, File::kRead);

  EXPECT_EQ(file.Size(), 33);
}

TEST(tl_io_file, Read) {
  File file;

  file.Open(Path{FLAGS_test_srcdir} / kASCIIFileName, File::kRead);

  std::array<char, 64> buffer;

  EXPECT_EQ(file.Read(buffer.data(), 7), 7);
  EXPECT_THAT(
      std::span(buffer).subspan(0, 7),
      Pointwise(Eq(), std::to_array({'A', 'S', 'C', 'I', 'I', ':', ' '})));

  EXPECT_EQ(file.Read(buffer.data(), 5), 5);
  EXPECT_THAT(std::span(buffer).subspan(0, 5),
              Pointwise(Eq(), std::to_array({'L', 'o', 'r', 'e', 'm'})));
}

TEST(tl_io_file, Write) {
  const Path filename = Path(FLAGS_test_srcdir) / "temp.txt";

  {
    File file;
    EXPECT_TRUE(file.Open(filename, File::kWrite | File::kCreateAlways));
    EXPECT_EQ(file.Write("Hello, World!", 13), 13);
  }

  {
    std::string text;

    EXPECT_TRUE(File::ReadText(filename, text));
    EXPECT_EQ(text, "Hello, World!");
  }

  EXPECT_TRUE(std::filesystem::remove(filename));
}

TEST(tl_io_file, IsEOR) {
  File file;

  file.Open(Path{FLAGS_test_srcdir} / kASCIIFileName, File::kRead);

  std::array<char, 64> buffer;

  EXPECT_FALSE(file.IsEOF());

  file.Read(buffer.data(), 5);
  EXPECT_FALSE(file.IsEOF());

  file.Read(buffer.data(), buffer.size());
  EXPECT_TRUE(file.IsEOF());
}

TEST(tl_io_file, ReadText) {
  File file;

  std::string text;

  EXPECT_TRUE(File::ReadText(Path{FLAGS_test_srcdir} / kASCIIFileName, text));
  EXPECT_EQ(text, "ASCII: Lorem ipsum dolor sit amet");
}

TEST(tl_io_file, ReadBytes) {
  std::vector<char> bytes;

  EXPECT_TRUE(File::ReadBytes(Path{FLAGS_test_srcdir} / kASCIIFileName, bytes));
  EXPECT_EQ(bytes.size(), 33);
  EXPECT_EQ(std::string(bytes.data(), bytes.size()),
            "ASCII: Lorem ipsum dolor sit amet");
}

TEST(tl_io_file, WriteText) {
  const Path filename = Path(FLAGS_test_srcdir) / "temp.txt";

  {
    File file;
    EXPECT_TRUE(file.WriteText(filename, std::string_view("Hello, World!")));
  }

  {
    std::string text;

    EXPECT_TRUE(File::ReadText(filename, text));
    EXPECT_EQ(text, "Hello, World!");
  }

  EXPECT_TRUE(std::filesystem::remove(filename));
}

TEST(tl_io_file, WriteBytes) {
  const Path filename = Path(FLAGS_test_srcdir) / "temp.txt";

  {
    File file;
    EXPECT_TRUE(file.WriteBytes(filename, std::string_view("Hello, World!")));
  }

  {
    std::string text;

    EXPECT_TRUE(File::ReadText(filename, text));
    EXPECT_EQ(text, "Hello, World!");
  }

  EXPECT_TRUE(std::filesystem::remove(filename));
}

// Test for read and write of a big buffer (over 4 GiB).
//
// Uses a lot of RAM and disk space, so is not enabled by default, but it is
// important to keep it tested on a bigger changes of the file implementation.
#if 0
TEST(tl_io_file, Big) {
  const Path filename = Path(FLAGS_test_srcdir) / "big.txt";

  {
    std::vector<uint8_t> data(size_t(5) * 1024 * 1024 * 1024 + 1);
    data.back() = 'X';

    EXPECT_TRUE(File::WriteBytes(filename, data));
  }

  {
    std::vector<uint8_t> data;
    EXPECT_TRUE(File::ReadBytes(filename, data));
    EXPECT_EQ(data.size(), size_t(5) * 1024 * 1024 * 1024 + 1);
    EXPECT_EQ(data.back(), 'X');
  }

  EXPECT_TRUE(std::filesystem::remove(filename));
}
#endif

}  // namespace tiny_lib::io_file
