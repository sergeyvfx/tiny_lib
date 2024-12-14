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
//
// Can be generated with the following Python script:
//
//   import numpy as np
//   from scipy.io import wavfile
//
//   stereo_samples = np.array(
//       (
//           np.array([0x0CCC, 0x3332], dtype=np.int16),
//           np.array([0x1999, 0x3FFF], dtype=np.int16),
//           np.array([0x2666, 0x4CCC], dtype=np.int16),
//       )
//   )
//
//   wavfile.write("out.wav", 44100, stereo_samples)
//
//  % xxd out.wav
//    00000000: 5249 4646 3000 0000 5741 5645 666d 7420  RIFF0...WAVEfmt
//    00000010: 1000 0000 0100 0200 44ac 0000 10b1 0200  ........D.......
//    00000020: 0400 1000 6461 7461 0c00 0000 cc0c 3233  ....data......23
//    00000030: 9919 ff3f 6626 cc4c                      ...?f&.L
struct RIFFWAV {
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

// Data of WAV file which uses RF64 format.
struct RF64WAV {
  static constexpr auto kData = std::to_array<uint8_t>({
      // clang-format off

      // RIFF chunk.
      'R', 'F' , '6' , '4',       // The RIFF chunk ID.
      0xff, 0xff, 0xff, 0xff,     // Chunk size.
      'W' , 'A' , 'V' , 'E',      // Format.

      // ds64 chunk.
      'd', 's', '6', '4',         // The ds64 chunk ID.
      0x1c, 0x00, 0x00, 0x00,     // Chunk size,
      0x54, 0x00, 0x00, 0x00,     // Low 4 byte size of RF64 block.
      0x00, 0x00, 0x00, 0x00,     // High 4 byte size of RF64 block.
      0x0c, 0x00, 0x00, 0x00,     // Low 4 byte size of data chunk.
      0x00, 0x00, 0x00, 0x00,     // High 4 byte size of data chunk.
      0x03, 0x00, 0x00, 0x00,     // Low 4 byte sample count of fact chunk.
      0x00, 0x00, 0x00, 0x00,     // High 4 byte sample count of fact chunk.
      0x00, 0x00, 0x00, 0x00,     // Number of valid entries in array “table”.

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
      0xff, 0xff, 0xff, 0xff,     // Chunk size (12 = 2 * 3 * 16bit samples).

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

// Minimalistic implementation of a file reader interface which reads data
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

////////////////////////////////////////////////////////////////////////////////
// GetFormatSpec().

static void TestCommonFormatSpec(const std::span<const uint8_t> data) {
  FileReaderFromMemory file_reader(data);
  Reader<FileReaderFromMemory> wav_reader;

  EXPECT_TRUE(wav_reader.Open(file_reader));

  const FormatSpec format_spec = wav_reader.GetFormatSpec();

  EXPECT_EQ(format_spec.num_channels, 2);
  EXPECT_EQ(format_spec.sample_rate, 44100);
  EXPECT_EQ(format_spec.bit_depth, 16);
}

TEST(tl_audio_wav_reader, FormatSpecRIFF) {
  TestCommonFormatSpec(RIFFWAV::kData);
}

TEST(tl_audio_wav_reader, FormatSpecRIF64) {
  TestCommonFormatSpec(RF64WAV::kData);
}

////////////////////////////////////////////////////////////////////////////////
// GetDurationInSeconds().

static void TestCommonGetDurationInSeconds(
    const std::span<const uint8_t> data) {
  FileReaderFromMemory file_reader(data);
  Reader<FileReaderFromMemory> wav_reader;

  EXPECT_TRUE(wav_reader.Open(file_reader));

  EXPECT_NEAR(wav_reader.GetDurationInSeconds(), 1.0f / 44100 * 3, 1e-8f);
}

TEST(tl_audio_wav_reader, GetDurationInSecondsRIFF) {
  TestCommonGetDurationInSeconds(RIFFWAV::kData);
}

TEST(tl_audio_wav_reader, GetDurationInSecondsRF64) {
  TestCommonGetDurationInSeconds(RF64WAV::kData);
}

////////////////////////////////////////////////////////////////////////////////
// ReadSingleSample(), ValueType=float.

static void TestCommonReadSingleSampleFloat(
    const std::span<const uint8_t> data,
    const std::span<const std::array<float, 2>> expected_samples,
    const float tolerance) {
  // Read into the buffer which matches the number of channels in the file.
  {
    FileReaderFromMemory file_reader(data);
    Reader<FileReaderFromMemory> wav_reader;

    EXPECT_TRUE(wav_reader.Open(file_reader));

    std::array<float, 2> sample;

    EXPECT_TRUE(wav_reader.ReadSingleSample<float>(sample));
    EXPECT_THAT(sample, Pointwise(FloatNear(tolerance), expected_samples[0]));

    EXPECT_TRUE(wav_reader.ReadSingleSample<float>(sample));
    EXPECT_THAT(sample, Pointwise(FloatNear(tolerance), expected_samples[1]));

    EXPECT_TRUE(wav_reader.ReadSingleSample<float>(sample));
    EXPECT_THAT(sample, Pointwise(FloatNear(tolerance), expected_samples[2]));

    EXPECT_FALSE(wav_reader.ReadSingleSample<float>(sample));
  }

  // Read into the buffer which is smaller than the number of channels.
  {
    FileReaderFromMemory file_reader(data);
    Reader<FileReaderFromMemory> wav_reader;

    EXPECT_TRUE(wav_reader.Open(file_reader));

    std::array<float, 1> sample;

    EXPECT_TRUE(wav_reader.ReadSingleSample<float>(sample));
    EXPECT_NEAR(sample[0], expected_samples[0][0], tolerance);

    EXPECT_TRUE(wav_reader.ReadSingleSample<float>(sample));
    EXPECT_NEAR(sample[0], expected_samples[1][0], tolerance);

    EXPECT_TRUE(wav_reader.ReadSingleSample<float>(sample));
    EXPECT_NEAR(sample[0], expected_samples[2][0], tolerance);

    EXPECT_FALSE(wav_reader.ReadSingleSample<float>(sample));
  }

  // Read into the buffer which is bigger than the number of channels.
  {
    FileReaderFromMemory file_reader(data);
    Reader<FileReaderFromMemory> wav_reader;

    EXPECT_TRUE(wav_reader.Open(file_reader));

    std::array<float, 4> sample;

    EXPECT_TRUE(wav_reader.ReadSingleSample<float>(sample));
    EXPECT_THAT(std::span(sample.data(), 2),
                Pointwise(FloatNear(tolerance), expected_samples[0]));

    EXPECT_TRUE(wav_reader.ReadSingleSample<float>(sample));
    EXPECT_THAT(std::span(sample.data(), 2),
                Pointwise(FloatNear(tolerance), expected_samples[1]));

    EXPECT_TRUE(wav_reader.ReadSingleSample<float>(sample));
    EXPECT_THAT(std::span(sample.data(), 2),
                Pointwise(FloatNear(tolerance), expected_samples[2]));

    EXPECT_FALSE(wav_reader.ReadSingleSample<float>(sample));
  }
}

TEST(tl_audio_wav_reader, ReadSingleSampleFloatRIFF) {
  TestCommonReadSingleSampleFloat(
      RIFFWAV::kData, RIFFWAV::kSamplesFloat, RIFFWAV::kTolerance);
}

TEST(tl_audio_wav_reader, ReadSingleSampleFloatRF64) {
  TestCommonReadSingleSampleFloat(
      RF64WAV::kData, RF64WAV::kSamplesFloat, RF64WAV::kTolerance);
}

////////////////////////////////////////////////////////////////////////////////
// ReadSingleSample(), ValueType=Int16.

static void TestCommonReadSingleSampleInt16(
    const std::span<const uint8_t> data,
    const std::span<const std::array<int16_t, 2>> expected_samples) {
  std::array<int16_t, 2> sample;

  // Read into the buffer which matches the number of channels in the file.
  {
    FileReaderFromMemory file_reader(data);
    Reader<FileReaderFromMemory> wav_reader;

    EXPECT_TRUE(wav_reader.Open(file_reader));

    EXPECT_TRUE(wav_reader.ReadSingleSample<int16_t>(sample));
    EXPECT_THAT(sample, Pointwise(Eq(), expected_samples[0]));

    EXPECT_TRUE(wav_reader.ReadSingleSample<int16_t>(sample));
    EXPECT_THAT(sample, Pointwise(Eq(), expected_samples[1]));

    EXPECT_TRUE(wav_reader.ReadSingleSample<int16_t>(sample));
    EXPECT_THAT(sample, Pointwise(Eq(), expected_samples[2]));

    EXPECT_FALSE(wav_reader.ReadSingleSample<int16_t>(sample));
  }

  // Read into the buffer which is smaller than the number of channels.
  {
    FileReaderFromMemory file_reader(data);
    Reader<FileReaderFromMemory> wav_reader;

    EXPECT_TRUE(wav_reader.Open(file_reader));

    EXPECT_TRUE(wav_reader.ReadSingleSample<int16_t>(sample));
    EXPECT_EQ(sample[0], expected_samples[0][0]);

    EXPECT_TRUE(wav_reader.ReadSingleSample<int16_t>(sample));
    EXPECT_EQ(sample[0], expected_samples[1][0]);

    EXPECT_TRUE(wav_reader.ReadSingleSample<int16_t>(sample));
    EXPECT_EQ(sample[0], expected_samples[2][0]);

    EXPECT_FALSE(wav_reader.ReadSingleSample<int16_t>(sample));
  }

  // Read into the buffer which is bigger than the number of channels.
  {
    FileReaderFromMemory file_reader(data);
    Reader<FileReaderFromMemory> wav_reader;

    EXPECT_TRUE(wav_reader.Open(file_reader));

    EXPECT_TRUE(wav_reader.ReadSingleSample<int16_t>(sample));
    EXPECT_THAT(std::span(sample.data(), 2),
                Pointwise(Eq(), expected_samples[0]));

    EXPECT_TRUE(wav_reader.ReadSingleSample<int16_t>(sample));
    EXPECT_THAT(std::span(sample.data(), 2),
                Pointwise(Eq(), expected_samples[1]));

    EXPECT_TRUE(wav_reader.ReadSingleSample<int16_t>(sample));
    EXPECT_THAT(std::span(sample.data(), 2),
                Pointwise(Eq(), expected_samples[2]));

    EXPECT_FALSE(wav_reader.ReadSingleSample<int16_t>(sample));
  }
}

TEST(tl_audio_wav_reader, ReadSingleSampleInt16RIFF) {
  TestCommonReadSingleSampleInt16(RIFFWAV::kData, RIFFWAV::kSamplesInt16);
}

TEST(tl_audio_wav_reader, ReadSingleSampleInt16RF64) {
  TestCommonReadSingleSampleInt16(RF64WAV::kData, RF64WAV::kSamplesInt16);
}

////////////////////////////////////////////////////////////////////////////////
// ReadAllSamples(), ValueType=float.

static void TestCommonReadAllSamplesFloat(
    const std::span<const uint8_t> data,
    const std::span<const std::array<float, 2>> expected_samples,
    const float tolerance) {
  FileReaderFromMemory file_reader(data);
  Reader<FileReaderFromMemory> wav_reader;

  EXPECT_TRUE(wav_reader.Open(file_reader));

  int sample_index = 0;
  const bool result = wav_reader.ReadAllSamples<float, 2>(
      [&](const std::span<const float> sample) {
        EXPECT_LT(sample_index, expected_samples.size());
        EXPECT_THAT(
            sample,
            Pointwise(FloatNear(tolerance), expected_samples[sample_index]));
        ++sample_index;
      });

  EXPECT_TRUE(result);
  EXPECT_EQ(sample_index, 3);
}

TEST(tl_audio_wav_reader, ReadAllSamplesFloatRIFF) {
  TestCommonReadAllSamplesFloat(
      RIFFWAV::kData, RIFFWAV::kSamplesFloat, RIFFWAV::kTolerance);
}

TEST(tl_audio_wav_reader, ReadAllSamplesFloatRF64) {
  TestCommonReadAllSamplesFloat(
      RF64WAV::kData, RF64WAV::kSamplesFloat, RF64WAV::kTolerance);
}

////////////////////////////////////////////////////////////////////////////////
// ReadAllSamples(), ValueType=int16_t.

static void TestCommonReadAllSamplesInt16(
    const std::span<const uint8_t> data,
    const std::span<const std::array<int16_t, 2>> expected_samples) {
  FileReaderFromMemory file_reader(data);
  Reader<FileReaderFromMemory> wav_reader;

  EXPECT_TRUE(wav_reader.Open(file_reader));

  int sample_index = 0;
  const bool result = wav_reader.ReadAllSamples<int16_t, 2>(
      [&](const std::span<const int16_t> sample) {
        EXPECT_LT(sample_index, expected_samples.size());
        EXPECT_THAT(sample, Pointwise(Eq(), expected_samples[sample_index]));
        ++sample_index;
      });

  EXPECT_TRUE(result);
  EXPECT_EQ(sample_index, 3);
}

TEST(tl_audio_wav_reader, ReadAllSamplesInt16RIFF) {
  TestCommonReadAllSamplesInt16(RIFFWAV::kData, RIFFWAV::kSamplesInt16);
}

TEST(tl_audio_wav_reader, ReadAllSamplesInt16RF64) {
  TestCommonReadAllSamplesInt16(RF64WAV::kData, RF64WAV::kSamplesInt16);
}

////////////////////////////////////////////////////////////////////////////////
// Simple pipeline.

TEST(tl_audio_wav_reader, SimplePipeline_lvalue) {
  FileReaderFromMemory file_reader(RIFFWAV::kData);

  int sample_index = 0;
  const bool result = Reader<FileReaderFromMemory>::ReadAllSamples<int16_t, 2>(
      file_reader, [&](const std::span<const int16_t> sample) {
        EXPECT_LT(sample_index, RIFFWAV::kSamplesInt16.size());
        EXPECT_THAT(sample,
                    Pointwise(Eq(), RIFFWAV::kSamplesInt16[sample_index]));
        ++sample_index;
      });

  EXPECT_TRUE(result);
  EXPECT_EQ(sample_index, 3);
}

TEST(tl_audio_wav_reader, SimplePipeline_rvalue) {
  int sample_index = 0;
  const bool result = Reader<FileReaderFromMemory>::ReadAllSamples<int16_t, 2>(
      FileReaderFromMemory(RIFFWAV::kData),
      [&](const std::span<const int16_t> sample) {
        EXPECT_LT(sample_index, RIFFWAV::kSamplesInt16.size());
        EXPECT_THAT(sample,
                    Pointwise(Eq(), RIFFWAV::kSamplesInt16[sample_index]));
        ++sample_index;
      });

  EXPECT_TRUE(result);
  EXPECT_EQ(sample_index, 3);
}

}  // namespace tiny_lib::audio_wav_reader
