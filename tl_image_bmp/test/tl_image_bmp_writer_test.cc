// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#include "tl_image_bmp/tl_image_bmp_writer.h"

#include <array>
#include <span>
#include <vector>

#include "tiny_lib/unittest/mock.h"
#include "tiny_lib/unittest/test.h"

namespace tiny_lib::image_bmp_writer {

using testing::Eq;
using testing::Pointwise;

class MemoryWriter {
 public:
  std::vector<uint8_t> storage;

  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  auto Write(const void* buffer, size_t num_bytes_to_write) -> size_t {
    std::span<const uint8_t> buffer_span(
        reinterpret_cast<const uint8_t*>(buffer), num_bytes_to_write);
    storage.insert(storage.end(), buffer_span.begin(), buffer_span.end());
    return num_bytes_to_write;
  };
};

TEST(tl_image_bmp_writer, Write_File24_Pixels3) {
  const FormatSpec format_spec = {
      .width = 9,
      .height = 2,
      .num_bits_per_pixel = 24,
  };

  std::vector<uint8_t> pixels(size_t(format_spec.width) * format_spec.height *
                              3);

  for (int i = 0; i < format_spec.width * format_spec.height; ++i) {
    const int channel_index = i * 3;
    pixels[channel_index + 0] = 32;
    pixels[channel_index + 1] = 64;
    pixels[channel_index + 2] = 128;
  }

  const PixelsSpec pixels_spec = {
      .num_channels = 3,
  };

  MemoryWriter memory_writer;
  Writer<MemoryWriter> writer;

  EXPECT_TRUE(writer.Open(memory_writer, format_spec));
  EXPECT_TRUE(writer.Write(pixels_spec, pixels));
  EXPECT_TRUE(writer.Close());

  // clang-format off
  EXPECT_THAT(
      memory_writer.storage,
      Pointwise(
          Eq(),
          std::to_array<uint8_t>({
              // Bitmap file header.
              0x42, 0x4d,              // BM
              0x6e, 0x00, 0x00, 0x00,  // Size of file
              0x00, 0x00,              // Reserved.
              0x00, 0x00,              // Reserved.
              0x36, 0x00, 0x00, 0x00,  // The offset.
              // Info header.
              0x28, 0x00, 0x00, 0x00,  // Size.
              0x09, 0x00, 0x00, 0x00,  // Width.
              0x02, 0x00, 0x00, 0x00,  // Height.
              0x01, 0x00,              // The number of color palettes.
              0x18, 0x00,              // Bits per pixel.
              0x00, 0x00, 0x00, 0x00,  // Compression.
              0x38, 0x00, 0x00, 0x00,  // The image size.
              0x00, 0x00, 0x00, 0x00,  // The horizontal resolution.
              0x00, 0x00, 0x00, 0x00,  // The vertical resolution.
              0x00, 0x00, 0x00, 0x00,  // The number of colors in the palette.
              0x00, 0x00, 0x00, 0x00,  // The number of important colors used.
              // Pixels.
              0x80, 0x40, 0x20,   0x80, 0x40, 0x20,
              0x80, 0x40, 0x20,   0x80, 0x40, 0x20,
              0x80, 0x40, 0x20,   0x80, 0x40, 0x20,
              0x80, 0x40, 0x20,   0x80, 0x40, 0x20,
              0x80, 0x40, 0x20,   0x00,  // Row 1
              0x80, 0x40, 0x20,   0x80, 0x40, 0x20,
              0x80, 0x40, 0x20,   0x80, 0x40, 0x20,
              0x80, 0x40, 0x20,   0x80, 0x40, 0x20,
              0x80, 0x40, 0x20,   0x80, 0x40, 0x20,
              0x80, 0x40, 0x20,   0x00,  // Row 2
          })));
  // clang-format on
}

TEST(tl_image_bmp_writer, Write_File24_Pixels4) {
  const FormatSpec format_spec = {
      .width = 9,
      .height = 2,
      .num_bits_per_pixel = 24,
  };

  std::vector<uint8_t> pixels(size_t(format_spec.width) * format_spec.height *
                              4);

  for (int i = 0; i < format_spec.width * format_spec.height; ++i) {
    const int channel_index = i * 4;
    pixels[channel_index + 0] = 32;
    pixels[channel_index + 1] = 64;
    pixels[channel_index + 2] = 128;
    pixels[channel_index + 3] = 255;
  }

  const PixelsSpec pixels_spec = {
      .num_channels = 4,
  };

  MemoryWriter memory_writer;
  Writer<MemoryWriter> writer;

  EXPECT_TRUE(writer.Open(memory_writer, format_spec));
  EXPECT_TRUE(writer.Write(pixels_spec, pixels));
  EXPECT_TRUE(writer.Close());

  // clang-format off
  EXPECT_THAT(
      memory_writer.storage,
      Pointwise(
          Eq(),
          std::to_array<uint8_t>({
              // Bitmap file header.
              0x42, 0x4d,              // BM
              0x6e, 0x00, 0x00, 0x00,  // Size of file
              0x00, 0x00,              // Reserved.
              0x00, 0x00,              // Reserved.
              0x36, 0x00, 0x00, 0x00,  // The offset.
              // Info header.
              0x28, 0x00, 0x00, 0x00,  // Size.
              0x09, 0x00, 0x00, 0x00,  // Width.
              0x02, 0x00, 0x00, 0x00,  // Height.
              0x01, 0x00,              // The number of color palettes.
              0x18, 0x00,              // Bits per pixel.
              0x00, 0x00, 0x00, 0x00,  // Compression.
              0x38, 0x00, 0x00, 0x00,  // The image size.
              0x00, 0x00, 0x00, 0x00,  // The horizontal resolution.
              0x00, 0x00, 0x00, 0x00,  // The vertical resolution.
              0x00, 0x00, 0x00, 0x00,  // The number of colors in the palette.
              0x00, 0x00, 0x00, 0x00,  // The number of important colors used.
              // Pixels.
              0x80, 0x40, 0x20,   0x80, 0x40, 0x20,
              0x80, 0x40, 0x20,   0x80, 0x40, 0x20,
              0x80, 0x40, 0x20,   0x80, 0x40, 0x20,
              0x80, 0x40, 0x20,   0x80, 0x40, 0x20,
              0x80, 0x40, 0x20,   0x00,   // Row 1
              0x80, 0x40, 0x20,   0x80, 0x40, 0x20,
              0x80, 0x40, 0x20,   0x80, 0x40, 0x20,
              0x80, 0x40, 0x20,   0x80, 0x40, 0x20,
              0x80, 0x40, 0x20,   0x80, 0x40, 0x20,
              0x80, 0x40, 0x20,   0x00,   // Row 2
          })));
  // clang-format on
}

TEST(tl_image_bmp_writer, Write_File32_Pixels3) {
  const FormatSpec format_spec = {
      .width = 9,
      .height = 2,
      .num_bits_per_pixel = 32,
  };

  std::vector<uint8_t> pixels(size_t(format_spec.width) * format_spec.height *
                              3);

  for (int i = 0; i < format_spec.width * format_spec.height; ++i) {
    const int channel_index = i * 3;
    pixels[channel_index + 0] = 32;
    pixels[channel_index + 1] = 64;
    pixels[channel_index + 2] = 128;
  }

  const PixelsSpec pixels_spec = {
      .num_channels = 3,
  };

  MemoryWriter memory_writer;
  Writer<MemoryWriter> writer;

  EXPECT_TRUE(writer.Open(memory_writer, format_spec));
  EXPECT_TRUE(writer.Write(pixels_spec, pixels));
  EXPECT_TRUE(writer.Close());

  // clang-format off
  EXPECT_THAT(
      memory_writer.storage,
      Pointwise(
          Eq(),
          std::to_array<uint8_t>({
              // Bitmap file header.
              0x42, 0x4d,              // BM
              0x7e, 0x00, 0x00, 0x00,  // Size of file
              0x00, 0x00,              // Reserved.
              0x00, 0x00,              // Reserved.
              0x36, 0x00, 0x00, 0x00,  // The offset.
              // Info header.
              0x28, 0x00, 0x00, 0x00,  // Size.
              0x09, 0x00, 0x00, 0x00,  // Width.
              0x02, 0x00, 0x00, 0x00,  // Height.
              0x01, 0x00,              // The number of color palettes.
              0x20, 0x00,              // Bits per pixel.
              0x00, 0x00, 0x00, 0x00,  // Compression.
              0x48, 0x00, 0x00, 0x00,  // The image size.
              0x00, 0x00, 0x00, 0x00,  // The horizontal resolution.
              0x00, 0x00, 0x00, 0x00,  // The vertical resolution.
              0x00, 0x00, 0x00, 0x00,  // The number of colors in the palette.
              0x00, 0x00, 0x00, 0x00,  // The number of important colors used.
              // Pixels.
              0x80, 0x40, 0x20, 0xff,   0x80, 0x40, 0x20, 0xff,
              0x80, 0x40, 0x20, 0xff,   0x80, 0x40, 0x20, 0xff,
              0x80, 0x40, 0x20, 0xff,   0x80, 0x40, 0x20, 0xff,
              0x80, 0x40, 0x20, 0xff,   0x80, 0x40, 0x20, 0xff,
              0x80, 0x40, 0x20, 0xff,   // Row 1
              0x80, 0x40, 0x20, 0xff,   0x80, 0x40, 0x20, 0xff,
              0x80, 0x40, 0x20, 0xff,   0x80, 0x40, 0x20, 0xff,
              0x80, 0x40, 0x20, 0xff,   0x80, 0x40, 0x20, 0xff,
              0x80, 0x40, 0x20, 0xff,   0x80, 0x40, 0x20, 0xff,
              0x80, 0x40, 0x20, 0xff,   // Row 2
          })));
  // clang-format on
}

TEST(tl_image_bmp_writer, Write_File32_Pixels4) {
  const FormatSpec format_spec = {
      .width = 9,
      .height = 2,
      .num_bits_per_pixel = 32,
  };

  std::vector<uint8_t> pixels(size_t(format_spec.width) * format_spec.height *
                              4);

  for (int i = 0; i < format_spec.width * format_spec.height; ++i) {
    const int channel_index = i * 4;
    pixels[channel_index + 0] = 32;
    pixels[channel_index + 1] = 64;
    pixels[channel_index + 2] = 128;
    pixels[channel_index + 3] = 255;
  }

  const PixelsSpec pixels_spec = {
      .num_channels = 4,
  };

  MemoryWriter memory_writer;
  Writer<MemoryWriter> writer;

  EXPECT_TRUE(writer.Open(memory_writer, format_spec));
  EXPECT_TRUE(writer.Write(pixels_spec, pixels));
  EXPECT_TRUE(writer.Close());

  // clang-format off
  EXPECT_THAT(
      memory_writer.storage,
      Pointwise(
          Eq(),
          std::to_array<uint8_t>({
              // Bitmap file header.
              0x42, 0x4d,              // BM
              0x7e, 0x00, 0x00, 0x00,  // Size of file
              0x00, 0x00,              // Reserved.
              0x00, 0x00,              // Reserved.
              0x36, 0x00, 0x00, 0x00,  // The offset.
              // Info header.
              0x28, 0x00, 0x00, 0x00,  // Size.
              0x09, 0x00, 0x00, 0x00,  // Width.
              0x02, 0x00, 0x00, 0x00,  // Height.
              0x01, 0x00,              // The number of color palettes.
              0x20, 0x00,              // Bits per pixel.
              0x00, 0x00, 0x00, 0x00,  // Compression.
              0x48, 0x00, 0x00, 0x00,  // The image size.
              0x00, 0x00, 0x00, 0x00,  // The horizontal resolution.
              0x00, 0x00, 0x00, 0x00,  // The vertical resolution.
              0x00, 0x00, 0x00, 0x00,  // The number of colors in the palette.
              0x00, 0x00, 0x00, 0x00,  // The number of important colors used.
              // Pixels.
              0x80, 0x40, 0x20, 0xff,   0x80, 0x40, 0x20, 0xff,
              0x80, 0x40, 0x20, 0xff,   0x80, 0x40, 0x20, 0xff,
              0x80, 0x40, 0x20, 0xff,   0x80, 0x40, 0x20, 0xff,
              0x80, 0x40, 0x20, 0xff,   0x80, 0x40, 0x20, 0xff,
              0x80, 0x40, 0x20, 0xff,   // Row 1
              0x80, 0x40, 0x20, 0xff,   0x80, 0x40, 0x20, 0xff,
              0x80, 0x40, 0x20, 0xff,   0x80, 0x40, 0x20, 0xff,
              0x80, 0x40, 0x20, 0xff,   0x80, 0x40, 0x20, 0xff,
              0x80, 0x40, 0x20, 0xff,   0x80, 0x40, 0x20, 0xff,
              0x80, 0x40, 0x20, 0xff,   // Row 2
          })));
  // clang-format on
}

}  // namespace tiny_lib::image_bmp_writer
