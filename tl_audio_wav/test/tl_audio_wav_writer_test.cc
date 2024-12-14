// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#include "tl_audio_wav/tl_audio_wav_writer.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <span>
#include <vector>

#include "tiny_lib/unittest/mock.h"
#include "tiny_lib/unittest/test.h"
#include "tl_io/tl_io_file.h"

#ifdef _MSC_VER
#  include <BaseTsd.h>
using ssize_t = SSIZE_T;
#endif

namespace tiny_lib::audio_wav_writer {

using testing::Eq;
using testing::Pointwise;

namespace {

template <std::endian>
struct SimpleWAV;

// Data of a simple WAV file.
// Stereo channel, 3 samples, 44100 samples per second.
template <>
struct SimpleWAV<std::endian::little> {
  // Floating point samples provided to the writer.
  static constexpr auto kSamplesFloat = std::to_array({
      std::array<float, 2>{0.1f, 0.4f},
      std::array<float, 2>{0.2f, 0.5f},
      std::array<float, 2>{0.3f, 0.6f},
  });
  static constexpr std::span<const float> kSamplesFloatFlat{
      kSamplesFloat[0].data(), sizeof(kSamplesFloat) / sizeof(float)};

  // Integral int16_t samples provided to the writer.
  static constexpr auto kSamplesInt16 = std::to_array({
      std::array<int16_t, 2>{3276, 13106},
      std::array<int16_t, 2>{6553, 16383},
      std::array<int16_t, 2>{9830, 19660},
  });
  static constexpr std::span<const int16_t> kSamplesInt16Flat{
      kSamplesInt16[0].data(), sizeof(kSamplesInt16) / sizeof(int16_t)};

  // Expected data tp be saved.
  static constexpr auto kData = std::to_array<uint8_t>({
      // clang-format off

      // RIFF chunk.
      'R', 'I' , 'F' , 'F',       // The RIFF chunk ID.
      0x30, 0x00, 0x00, 0x00,     // Chunk size.
      'W' , 'A' , 'V' , 'E',      // Format.

      // FMT chunk.
      'f', 'm' , 't' , ' ',       // The FMT chunk ID.
      0x10, 0x00, 0x00, 0x00,     // Chunk size (16 for PCM).
      0x01, 0x00,                 // Audio format (1 for PCM),
      0x02, 0x00,                 // Number of channels.
      0x44, 0xac, 0x00, 0x00,     // Sample rate (hex(44100) -> 0xac44).
      0x10, 0xb1, 0x02, 0x00,     // Byte rate (hex(44100 * 2 * 2) -> 0x2b110)
      0x04, 0x00,                 // Block align.
      0x10, 0x00,                 // Bits per sample.

      // DATA chunk.
      'd' , 'a' , 't' , 'a' ,     // The DATA chunk ID.
      0x0c, 0x00, 0x00, 0x00,     // Chunk size (12 = 2 * 3 * 16bit samples).

      // Samples.
      0xcc, 0x0c,  0x32, 0x33,    // Sample 1: (0.1, 0.4).
      0x99, 0x19,  0xff, 0x3f,    // Sample 2: (0.2, 0.5).
      0x66, 0x26,  0xcc, 0x4c,    // Sample 3: (0.3, 0.6).
      // clang-format on
  });
};

// Minimalistic implementation of a file writer interface which writes data to
// an in-memory container.
class FileWriterToMemory {
 public:
  explicit FileWriterToMemory(std::vector<uint8_t>& memory_buffer)
      : memory_buffer_(memory_buffer) {}

  auto Rewind() -> bool {
    position_ = 0;
    return true;
  }

  auto Write(const void* ptr, const int num_bytes_to_write) -> int {
    // Make sure the storage is big enough.
    const int required_size = position_ + num_bytes_to_write;
    if (required_size > memory_buffer_.size()) {
      memory_buffer_.resize(required_size);
    }

    // Helper class to act as a generator for `std::generate()` which returns
    // bytes from the buffer to be written.
    class DataGenerator {
     public:
      DataGenerator(const void* ptr, const int size)
          : buffer_(static_cast<const uint8_t*>(ptr), size) {}

      auto operator()() -> uint8_t {
        assert(current_index_ < buffer_.size());
        return buffer_[current_index_++];
      }

     private:
      const std::span<const uint8_t> buffer_;
      int current_index_ = 0;
    };

    // Assign values in the container to the ones which are to be written.
    std::generate(memory_buffer_.begin() + position_,
                  memory_buffer_.begin() + position_ + num_bytes_to_write,
                  DataGenerator(ptr, num_bytes_to_write));

    // Advance the write head position.
    position_ += num_bytes_to_write;

    return num_bytes_to_write;
  }

 private:
  std::vector<uint8_t>& memory_buffer_;

  int position_{0};
};

class SimpleFileWriterToMemory : public FileWriterToMemory {
 public:
  std::vector<uint8_t> buffer;

  SimpleFileWriterToMemory() : FileWriterToMemory(buffer) {}
};

}  // namespace

TEST(tl_audio_wav_writer, MaxNumSamples) {
  {
    EXPECT_EQ(Writer<SimpleFileWriterToMemory>::MaxNumSamples(FormatSpec{
                  .num_channels = 2, .sample_rate = 44100, .bit_depth = 16}),
              0x3ffffff6);
  }
}

TEST(tl_audio_wav_writer, WriteSingleSample) {
  using SimpleWAV = SimpleWAV<std::endian::native>;

  const FormatSpec format_spec = {
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_depth = 16,
  };

  // float source samples.
  {
    SimpleFileWriterToMemory file_writer;
    Writer<SimpleFileWriterToMemory> wav_writer;

    EXPECT_TRUE(wav_writer.Open(file_writer, format_spec));

    for (const auto& sample : SimpleWAV::kSamplesFloat) {
      EXPECT_TRUE(wav_writer.WriteSingleSample<float>(sample));
    }

    EXPECT_TRUE(wav_writer.Close());

    EXPECT_THAT(file_writer.buffer, Pointwise(Eq(), SimpleWAV::kData));
  }

  // int16_t source samples.
  {
    SimpleFileWriterToMemory file_writer;
    Writer<SimpleFileWriterToMemory> wav_writer;

    EXPECT_TRUE(wav_writer.Open(file_writer, format_spec));

    for (const auto& sample : SimpleWAV::kSamplesInt16) {
      EXPECT_TRUE(wav_writer.WriteSingleSample<int16_t>(sample));
    }

    EXPECT_TRUE(wav_writer.Close());

    EXPECT_THAT(file_writer.buffer, Pointwise(Eq(), SimpleWAV::kData));
  }
}

TEST(tl_audio_wav_writer, WriteMultipleSamples) {
  using SimpleWAV = SimpleWAV<std::endian::native>;

  const FormatSpec format_spec = {
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_depth = 16,
  };

  // float source samples.
  {
    SimpleFileWriterToMemory file_writer;
    Writer<SimpleFileWriterToMemory> wav_writer;

    EXPECT_TRUE(wav_writer.Open(file_writer, format_spec));
    EXPECT_TRUE(wav_writer.WriteMultipleSamples(SimpleWAV::kSamplesFloatFlat));
    EXPECT_TRUE(wav_writer.Close());
    EXPECT_THAT(file_writer.buffer, Pointwise(Eq(), SimpleWAV::kData));
  }

  // int16_t source samples.
  {
    SimpleFileWriterToMemory file_writer;
    Writer<SimpleFileWriterToMemory> wav_writer;

    EXPECT_TRUE(wav_writer.Open(file_writer, format_spec));
    EXPECT_TRUE(wav_writer.WriteMultipleSamples(SimpleWAV::kSamplesInt16Flat));
    EXPECT_TRUE(wav_writer.Close());
    EXPECT_THAT(file_writer.buffer, Pointwise(Eq(), SimpleWAV::kData));
  }
}

TEST(tl_audio_wav_writer, SimplePipeline) {
  using SimpleWAV = SimpleWAV<std::endian::native>;

  const FormatSpec format_spec = {
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_depth = 16,
  };

  // float source samples.
  {
    SimpleFileWriterToMemory file_writer;

    EXPECT_TRUE(Writer<SimpleFileWriterToMemory>::Write(
        file_writer, format_spec, SimpleWAV::kSamplesFloatFlat));

    EXPECT_THAT(file_writer.buffer, Pointwise(Eq(), SimpleWAV::kData));
  }

  // int16_t source samples.
  {
    SimpleFileWriterToMemory file_writer;

    EXPECT_TRUE(Writer<SimpleFileWriterToMemory>::Write(
        file_writer, format_spec, SimpleWAV::kSamplesInt16Flat));

    EXPECT_THAT(file_writer.buffer, Pointwise(Eq(), SimpleWAV::kData));
  }
}

// Ensure that tiny_lib::io_file::File implements needed APIs.
TEST(tl_audio_wav_writer, File) {
  io_file::File file;
  Writer<io_file::File> wav_writer;

  if (false) {  // NOLINT(readability-simplify-boolean-expr)
    const FormatSpec format_spec = {
        .num_channels = 2,
        .sample_rate = 44100,
        .bit_depth = 16,
    };
    wav_writer.Open(file, format_spec);
    wav_writer.Close();
  }
}

}  // namespace tiny_lib::audio_wav_writer
