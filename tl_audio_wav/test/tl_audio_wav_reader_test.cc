// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#include "tl_audio_wav/tl_audio_wav_reader.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

#include "tiny_lib/unittest/mock.h"
#include "tiny_lib/unittest/test.h"

#ifdef _MSC_VER
#  include <BaseTsd.h>
using ssize_t = SSIZE_T;
#endif

namespace tiny_lib::audio_wav_reader {

using testing::Eq;
using testing::FloatNear;
using testing::Pointwise;

namespace {

// Data of a simple WAV file.
// Stereo channel, 3 samples, 44100 samples per second.
struct SimpleWAV {
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

  // Tolerance when comparing samples read as floating point.
  static constexpr float kTolerance = 1.0f / 32767;

  // Expected values of samples when they are read as float.
  static constexpr auto kSamplesFloat = std::to_array({
      std::array<float, 2>{0.1f, 0.4f},
      std::array<float, 2>{0.2f, 0.5f},
      std::array<float, 2>{0.3f, 0.6f},
  });

  // Expected values of samples when they are read as int16_t.
  static constexpr auto kSamplesInt16 = std::to_array({
      std::array<int16_t, 2>{3276, 13106},
      std::array<int16_t, 2>{6553, 16383},
      std::array<int16_t, 2>{9830, 19660},
  });
};

// Minimaliztic implementation of a file reader interface which reads data
// from an in-memory container.
class FileReaderFromMemory {
 public:
  explicit FileReaderFromMemory(const std::span<const uint8_t> memory_buffer)
      : memory_buffer_(memory_buffer),
        memory_buffer_size_(int(memory_buffer_.size())) {
    assert(memory_buffer_.size() <= std::numeric_limits<int>::max());
  }

  // The return value matches the behavior of POSIX read():
  //   - Upon success the number of bytes actually read is returned.
  //   - Upon reading past the end-of-file, zero is returned.
  //   - Otherwise, -1 is returned.
  auto Read(void* ptr, const int num_bytes_to_read) -> int {
    if (position_ >= memory_buffer_size_) {
      return 0;
    }

    int num_bytes_read =
        std::min(num_bytes_to_read, memory_buffer_size_ - position_);

    memcpy(ptr, memory_buffer_.data() + position_, num_bytes_read);

    position_ += num_bytes_read;

    return num_bytes_read;
  }

 private:
  std::span<const uint8_t> memory_buffer_;
  int memory_buffer_size_{0};
  int position_{0};
};

}  // namespace

TEST(tl_audio_wav_reader, FormatSpec) {
  FileReaderFromMemory file_reader(SimpleWAV::kData);
  Reader<FileReaderFromMemory> wav_reader;

  EXPECT_TRUE(wav_reader.Open(file_reader));

  const FormatSpec format_spec = wav_reader.GetFormatSpec();

  EXPECT_EQ(format_spec.num_channels, 2);
  EXPECT_EQ(format_spec.sample_rate, 44100);
  EXPECT_EQ(format_spec.bit_depth, 16);
}

TEST(tl_audio_wav_reader, GetDurationInSeconds) {
  FileReaderFromMemory file_reader(SimpleWAV::kData);
  Reader<FileReaderFromMemory> wav_reader;

  EXPECT_TRUE(wav_reader.Open(file_reader));

  EXPECT_NEAR(wav_reader.GetDurationInSeconds(), 1.0f / 44100 * 3, 1e-8f);
}

TEST(tl_audio_wav_reader, ReadSingleSample_Float) {
  // Read into the buffer which matches the number of channels in the file.
  {
    FileReaderFromMemory file_reader(SimpleWAV::kData);
    Reader<FileReaderFromMemory> wav_reader;

    EXPECT_TRUE(wav_reader.Open(file_reader));

    std::array<float, 2> sample;

    EXPECT_TRUE(wav_reader.ReadSingleSample<float>(sample));
    EXPECT_THAT(sample,
                Pointwise(FloatNear(SimpleWAV::kTolerance),
                          SimpleWAV::kSamplesFloat[0]));

    EXPECT_TRUE(wav_reader.ReadSingleSample<float>(sample));
    EXPECT_THAT(sample,
                Pointwise(FloatNear(SimpleWAV::kTolerance),
                          SimpleWAV::kSamplesFloat[1]));

    EXPECT_TRUE(wav_reader.ReadSingleSample<float>(sample));
    EXPECT_THAT(sample,
                Pointwise(FloatNear(SimpleWAV::kTolerance),
                          SimpleWAV::kSamplesFloat[2]));

    EXPECT_FALSE(wav_reader.ReadSingleSample<float>(sample));
  }

  // Read into the buffer which is smaller than the number of channels.
  {
    FileReaderFromMemory file_reader(SimpleWAV::kData);
    Reader<FileReaderFromMemory> wav_reader;

    EXPECT_TRUE(wav_reader.Open(file_reader));

    std::array<float, 1> sample;

    EXPECT_TRUE(wav_reader.ReadSingleSample<float>(sample));
    EXPECT_NEAR(
        sample[0], SimpleWAV::kSamplesFloat[0][0], SimpleWAV::kTolerance);

    EXPECT_TRUE(wav_reader.ReadSingleSample<float>(sample));
    EXPECT_NEAR(
        sample[0], SimpleWAV::kSamplesFloat[1][0], SimpleWAV::kTolerance);

    EXPECT_TRUE(wav_reader.ReadSingleSample<float>(sample));
    EXPECT_NEAR(
        sample[0], SimpleWAV::kSamplesFloat[2][0], SimpleWAV::kTolerance);

    EXPECT_FALSE(wav_reader.ReadSingleSample<float>(sample));
  }

  // Read into the buffer which is bigger than the number of channels.
  {
    FileReaderFromMemory file_reader(SimpleWAV::kData);
    Reader<FileReaderFromMemory> wav_reader;

    EXPECT_TRUE(wav_reader.Open(file_reader));

    std::array<float, 4> sample;

    EXPECT_TRUE(wav_reader.ReadSingleSample<float>(sample));
    EXPECT_THAT(std::span(sample.data(), 2),
                Pointwise(FloatNear(SimpleWAV::kTolerance),
                          SimpleWAV::kSamplesFloat[0]));

    EXPECT_TRUE(wav_reader.ReadSingleSample<float>(sample));
    EXPECT_THAT(std::span(sample.data(), 2),
                Pointwise(FloatNear(SimpleWAV::kTolerance),
                          SimpleWAV::kSamplesFloat[1]));

    EXPECT_TRUE(wav_reader.ReadSingleSample<float>(sample));
    EXPECT_THAT(std::span(sample.data(), 2),
                Pointwise(FloatNear(SimpleWAV::kTolerance),
                          SimpleWAV::kSamplesFloat[2]));

    EXPECT_FALSE(wav_reader.ReadSingleSample<float>(sample));
  }
}

TEST(tl_audio_wav_reader, ReadSingleSample_Int16) {
  std::array<int16_t, 2> sample;

  // Read into the buffer which matches the number of channels in the file.
  {
    FileReaderFromMemory file_reader(SimpleWAV::kData);
    Reader<FileReaderFromMemory> wav_reader;

    EXPECT_TRUE(wav_reader.Open(file_reader));

    EXPECT_TRUE(wav_reader.ReadSingleSample<int16_t>(sample));
    EXPECT_THAT(sample, Pointwise(Eq(), SimpleWAV::kSamplesInt16[0]));

    EXPECT_TRUE(wav_reader.ReadSingleSample<int16_t>(sample));
    EXPECT_THAT(sample, Pointwise(Eq(), SimpleWAV::kSamplesInt16[1]));

    EXPECT_TRUE(wav_reader.ReadSingleSample<int16_t>(sample));
    EXPECT_THAT(sample, Pointwise(Eq(), SimpleWAV::kSamplesInt16[2]));

    EXPECT_FALSE(wav_reader.ReadSingleSample<int16_t>(sample));
  }

  // Read into the buffer which is smaller than the number of channels.
  {
    FileReaderFromMemory file_reader(SimpleWAV::kData);
    Reader<FileReaderFromMemory> wav_reader;

    EXPECT_TRUE(wav_reader.Open(file_reader));

    EXPECT_TRUE(wav_reader.ReadSingleSample<int16_t>(sample));
    EXPECT_EQ(sample[0], SimpleWAV::kSamplesInt16[0][0]);

    EXPECT_TRUE(wav_reader.ReadSingleSample<int16_t>(sample));
    EXPECT_EQ(sample[0], SimpleWAV::kSamplesInt16[1][0]);

    EXPECT_TRUE(wav_reader.ReadSingleSample<int16_t>(sample));
    EXPECT_EQ(sample[0], SimpleWAV::kSamplesInt16[2][0]);

    EXPECT_FALSE(wav_reader.ReadSingleSample<int16_t>(sample));
  }

  // Read into the buffer which is bigger than the number of channels.
  {
    FileReaderFromMemory file_reader(SimpleWAV::kData);
    Reader<FileReaderFromMemory> wav_reader;

    EXPECT_TRUE(wav_reader.Open(file_reader));

    EXPECT_TRUE(wav_reader.ReadSingleSample<int16_t>(sample));
    EXPECT_THAT(std::span(sample.data(), 2),
                Pointwise(Eq(), SimpleWAV::kSamplesInt16[0]));

    EXPECT_TRUE(wav_reader.ReadSingleSample<int16_t>(sample));
    EXPECT_THAT(std::span(sample.data(), 2),
                Pointwise(Eq(), SimpleWAV::kSamplesInt16[1]));

    EXPECT_TRUE(wav_reader.ReadSingleSample<int16_t>(sample));
    EXPECT_THAT(std::span(sample.data(), 2),
                Pointwise(Eq(), SimpleWAV::kSamplesInt16[2]));

    EXPECT_FALSE(wav_reader.ReadSingleSample<int16_t>(sample));
  }
}

TEST(tl_audio_wav_reader, ReadAllSamples_Float) {
  FileReaderFromMemory file_reader(SimpleWAV::kData);
  Reader<FileReaderFromMemory> wav_reader;

  EXPECT_TRUE(wav_reader.Open(file_reader));

  int sample_index = 0;
  const bool result = wav_reader.ReadAllSamples<float, 2>(
      [&](const std::span<const float> sample) {
        EXPECT_LT(sample_index, SimpleWAV::kSamplesFloat.size());
        EXPECT_THAT(sample,
                    Pointwise(FloatNear(SimpleWAV::kTolerance),
                              SimpleWAV::kSamplesFloat[sample_index]));
        ++sample_index;
      });

  EXPECT_TRUE(result);
  EXPECT_EQ(sample_index, 3);
}

TEST(tl_audio_wav_reader, ReadAllSamples_Int16) {
  FileReaderFromMemory file_reader(SimpleWAV::kData);
  Reader<FileReaderFromMemory> wav_reader;

  EXPECT_TRUE(wav_reader.Open(file_reader));

  int sample_index = 0;
  const bool result = wav_reader.ReadAllSamples<int16_t, 2>(
      [&](const std::span<const int16_t> sample) {
        EXPECT_LT(sample_index, SimpleWAV::kSamplesInt16.size());
        EXPECT_THAT(sample,
                    Pointwise(Eq(), SimpleWAV::kSamplesInt16[sample_index]));
        ++sample_index;
      });

  EXPECT_TRUE(result);
  EXPECT_EQ(sample_index, 3);
}

TEST(tl_audio_wav_reader, SimplePipeline_lvalue) {
  FileReaderFromMemory file_reader(SimpleWAV::kData);

  int sample_index = 0;
  const bool result = Reader<FileReaderFromMemory>::ReadAllSamples<int16_t, 2>(
      file_reader, [&](const std::span<const int16_t> sample) {
        EXPECT_LT(sample_index, SimpleWAV::kSamplesInt16.size());
        EXPECT_THAT(sample,
                    Pointwise(Eq(), SimpleWAV::kSamplesInt16[sample_index]));
        ++sample_index;
      });

  EXPECT_TRUE(result);
  EXPECT_EQ(sample_index, 3);
}

TEST(tl_audio_wav_reader, SimplePipeline_rvalue) {
  int sample_index = 0;
  const bool result = Reader<FileReaderFromMemory>::ReadAllSamples<int16_t, 2>(
      FileReaderFromMemory(SimpleWAV::kData),
      [&](const std::span<const int16_t> sample) {
        EXPECT_LT(sample_index, SimpleWAV::kSamplesInt16.size());
        EXPECT_THAT(sample,
                    Pointwise(Eq(), SimpleWAV::kSamplesInt16[sample_index]));
        ++sample_index;
      });

  EXPECT_TRUE(result);
  EXPECT_EQ(sample_index, 3);
}

}  // namespace tiny_lib::audio_wav_reader
