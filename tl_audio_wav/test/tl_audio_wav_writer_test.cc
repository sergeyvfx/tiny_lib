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
#include "tl_audio_wav/test/tl_audio_wav_test_data.h"
#include "tl_io/tl_io_file.h"

namespace tiny_lib::audio_wav_writer {

using testing::Eq;
using testing::Pointwise;

using namespace audio_wav_test_data;

namespace {

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
  // RIFF.
  {
    EXPECT_EQ(Writer<SimpleFileWriterToMemory>::MaxNumSamples(FormatSpec{
                  .file_format = FileFormat::kRIFF,
                  .num_channels = 2,
                  .sample_rate = 44100,
                  .bit_depth = 16,
              }),
              0x3ffffff6);
  }
  // RF64.
  {
    EXPECT_EQ(Writer<SimpleFileWriterToMemory>::MaxNumSamples(FormatSpec{
                  .file_format = FileFormat::kRF64,
                  .num_channels = 2,
                  .sample_rate = 44100,
                  .bit_depth = 16,
              }),
              0x3fffffffffffffed);
  }
}

static void TestCommonWriteSingleSample(const FormatSpec& format_spec,
                                        const TestData& test_data) {
  // float source samples.
  {
    SimpleFileWriterToMemory file_writer;
    Writer<SimpleFileWriterToMemory> wav_writer;

    EXPECT_TRUE(wav_writer.Open(file_writer, format_spec));

    for (const auto& sample : test_data.GetSamplesFloat()) {
      EXPECT_TRUE(wav_writer.WriteSingleSample<float>(sample));
    }

    EXPECT_TRUE(wav_writer.Close());

    EXPECT_THAT(file_writer.buffer, Pointwise(Eq(), test_data.GetData()));
  }

  // int16_t source samples.
  {
    SimpleFileWriterToMemory file_writer;
    Writer<SimpleFileWriterToMemory> wav_writer;

    EXPECT_TRUE(wav_writer.Open(file_writer, format_spec));

    for (const auto& sample : test_data.GetSamplesInt16()) {
      EXPECT_TRUE(wav_writer.WriteSingleSample<int16_t>(sample));
    }

    EXPECT_TRUE(wav_writer.Close());

    EXPECT_THAT(file_writer.buffer, Pointwise(Eq(), test_data.GetData()));
  }
}

TEST(tl_audio_wav_writer, WriteSingleSample_RIFFWAV_PCM16) {
  ASSERT_TRUE(std::endian::native == std::endian::little)
      << "Code is only tested on little endian machines";

  constexpr FormatSpec kFormatSpec = {
      .file_format = FileFormat::kRIFF,
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_depth = 16,
  };

  TestCommonWriteSingleSample(kFormatSpec, RIFFWAV_PCM16());
}

TEST(tl_audio_wav_writer, WriteSingleSample_RF64WAV_PCM16) {
  ASSERT_TRUE(std::endian::native == std::endian::little)
      << "Code is only tested on little endian machines";

  constexpr FormatSpec kFormatSpec = {
      .file_format = FileFormat::kRF64,
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_depth = 16,
  };

  TestCommonWriteSingleSample(kFormatSpec, RF64WAV_PCM16());
}

static void TestCommonWriteMultipleSamples(const FormatSpec& format_spec,
                                           const TestData& test_data) {
  // float source samples.
  {
    SimpleFileWriterToMemory file_writer;
    Writer<SimpleFileWriterToMemory> wav_writer;

    EXPECT_TRUE(wav_writer.Open(file_writer, format_spec));
    EXPECT_TRUE(wav_writer.WriteMultipleSamples(test_data.GetSamplesFloat()));
    EXPECT_TRUE(wav_writer.Close());
    EXPECT_THAT(file_writer.buffer, Pointwise(Eq(), test_data.GetData()));
  }

  // int16_t source samples.
  {
    SimpleFileWriterToMemory file_writer;
    Writer<SimpleFileWriterToMemory> wav_writer;

    EXPECT_TRUE(wav_writer.Open(file_writer, format_spec));
    EXPECT_TRUE(wav_writer.WriteMultipleSamples(test_data.GetSamplesInt16()));
    EXPECT_TRUE(wav_writer.Close());
    EXPECT_THAT(file_writer.buffer, Pointwise(Eq(), test_data.GetData()));
  }
}

TEST(tl_audio_wav_writer, WriteMultipleSamples_RIFFWAV_PCM16) {
  ASSERT_TRUE(std::endian::native == std::endian::little)
      << "Code is only tested on little endian machines";

  constexpr FormatSpec kFormatSpec = {
      .file_format = FileFormat::kRIFF,
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_depth = 16,
  };

  TestCommonWriteMultipleSamples(kFormatSpec, RIFFWAV_PCM16());
}

TEST(tl_audio_wav_writer, WriteMultipleSamples_RF64WAV_PCM16) {
  ASSERT_TRUE(std::endian::native == std::endian::little)
      << "Code is only tested on little endian machines";

  constexpr FormatSpec kFormatSpec = {
      .file_format = FileFormat::kRF64,
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_depth = 16,
  };

  TestCommonWriteMultipleSamples(kFormatSpec, RF64WAV_PCM16());
}

static void TestCommonSimplePipeline(const FormatSpec& format_spec,
                                     const TestData& test_data) {
  // float source samples.
  {
    SimpleFileWriterToMemory file_writer;

    EXPECT_TRUE(Writer<SimpleFileWriterToMemory>::Write(
        file_writer, format_spec, test_data.GetSamplesFloat()));

    EXPECT_THAT(file_writer.buffer, Pointwise(Eq(), test_data.GetData()));
  }

  // int16_t source samples.
  {
    SimpleFileWriterToMemory file_writer;

    EXPECT_TRUE(Writer<SimpleFileWriterToMemory>::Write(
        file_writer, format_spec, test_data.GetSamplesInt16()));

    EXPECT_THAT(file_writer.buffer, Pointwise(Eq(), test_data.GetData()));
  }
}

TEST(tl_audio_wav_writer, SimplePipeline_RIFFWAV_PCM16) {
  ASSERT_TRUE(std::endian::native == std::endian::little)
      << "Code is only tested on little endian machines";

  constexpr FormatSpec kFormatSpec = {
      .file_format = FileFormat::kRIFF,
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_depth = 16,
  };

  TestCommonSimplePipeline(kFormatSpec, RIFFWAV_PCM16());
}

TEST(tl_audio_wav_writer, SimplePipeline_RF64WAV_PCM16) {
  ASSERT_TRUE(std::endian::native == std::endian::little)
      << "Code is only tested on little endian machines";

  constexpr FormatSpec kFormatSpec = {
      .file_format = FileFormat::kRF64,
      .num_channels = 2,
      .sample_rate = 44100,
      .bit_depth = 16,
  };

  TestCommonSimplePipeline(kFormatSpec, RF64WAV_PCM16());
}

// Ensure that tiny_lib::io_file::File implements needed APIs.
TEST(tl_audio_wav_writer, File) {
  io_file::File file;
  Writer<io_file::File> wav_writer;

  if (false) {  // NOLINT(readability-simplify-boolean-expr)
    constexpr FormatSpec kFormatSpec = {
        .file_format = FileFormat::kRIFF,
        .num_channels = 2,
        .sample_rate = 44100,
        .bit_depth = 16,
    };
    wav_writer.Open(file, kFormatSpec);
    wav_writer.Close();
  }
}

}  // namespace tiny_lib::audio_wav_writer
