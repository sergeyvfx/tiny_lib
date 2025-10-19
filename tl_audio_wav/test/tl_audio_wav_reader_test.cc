// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#include "tl_audio_wav/tl_audio_wav_reader.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <span>

#include "tiny_lib/unittest/mock.h"
#include "tiny_lib/unittest/test.h"
#include "tl_audio_wav/test/tl_audio_wav_test_data.h"

namespace tiny_lib::audio_wav_reader {

using testing::Eq;
using testing::FloatNear;
using testing::Pointwise;

using namespace audio_wav_test_data;

////////////////////////////////////////////////////////////////////////////////
// Byteswap().

TEST(Byteswap, Scalar) {
  EXPECT_EQ(internal::Byteswap(uint16_t(0x1234)), 0x3412);
  EXPECT_EQ(internal::Byteswap(uint32_t(0x12345678)), 0x78563412);
  EXPECT_EQ(internal::Byteswap(uint64_t(0x1234567890abcdef)),
            0xefcdab9078563412);
}

namespace {

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

static void TestCommonFormatSpec(const TestData& test_data) {
  FileReaderFromMemory file_reader(test_data.GetData());
  Reader<FileReaderFromMemory> wav_reader;

  EXPECT_TRUE(wav_reader.Open(file_reader));

  const FormatSpec format_spec = wav_reader.GetFormatSpec();

  EXPECT_EQ(format_spec.num_channels, 2);
  EXPECT_EQ(format_spec.sample_rate, 44100);
  EXPECT_EQ(format_spec.bit_depth, test_data.GetBitDepth());
}

TEST(tl_audio_wav_reader, FormatSpec_RIFFWAV_PCM16) {
  TestCommonFormatSpec(RIFFWAV_PCM16());
}

TEST(tl_audio_wav_reader, FormatSpec_RIFFWAV_FLOAT) {
  TestCommonFormatSpec(RIFFWAV_FLOAT());
}

TEST(tl_audio_wav_reader, FormatSpec_RIFF_DOUBLE) {
  TestCommonFormatSpec(RIFFWAV_DOUBLE());
}

TEST(tl_audio_wav_reader, FormatSpec_RF64WAV_PCM16) {
  TestCommonFormatSpec(RF64WAV_PCM16());
}

TEST(tl_audio_wav_reader, FormatSpec_RF64WAV_EXTENSIBLE_PCM16) {
  TestCommonFormatSpec(RF64WAV_EXTENSIBLE_PCM16());
}

TEST(tl_audio_wav_reader, FormatSpec_RF64WAV_EXTENSIBLE_FLOAT) {
  TestCommonFormatSpec(RF64WAV_EXTENSIBLE_FLOAT());
}

TEST(tl_audio_wav_reader, FormatSpec_RF64WAV_EXTENSIBLE_DOUBLE) {
  TestCommonFormatSpec(RF64WAV_EXTENSIBLE_DOUBLE());
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

TEST(tl_audio_wav_reader, GetDurationInSeconds_RIFFWAV_PCM16) {
  TestCommonGetDurationInSeconds(RIFFWAV_PCM16::kData);
}

TEST(tl_audio_wav_reader, GetDurationInSeconds_RIFFWAV_FLOAT) {
  TestCommonGetDurationInSeconds(RIFFWAV_FLOAT::kData);
}

TEST(tl_audio_wav_reader, GetDurationInSeconds_RIFF_DOUBLE) {
  TestCommonGetDurationInSeconds(RIFFWAV_DOUBLE::kData);
}

TEST(tl_audio_wav_reader, GetDurationInSeconds_RF64WAV_PCM16) {
  TestCommonGetDurationInSeconds(RF64WAV_PCM16::kData);
}

TEST(tl_audio_wav_reader, GetDurationInSeconds_RF64WAV_EXTENSIBLE_PCM16) {
  TestCommonGetDurationInSeconds(RF64WAV_EXTENSIBLE_PCM16::kData);
}

TEST(tl_audio_wav_reader, GetDurationInSeconds_RF64WAV_EXTENSIBLE_FLOAT) {
  TestCommonGetDurationInSeconds(RF64WAV_EXTENSIBLE_FLOAT::kData);
}

TEST(tl_audio_wav_reader, GetDurationInSeconds_RF64WAV_EXTENSIBLE_DOUBLE) {
  TestCommonGetDurationInSeconds(RF64WAV_EXTENSIBLE_DOUBLE::kData);
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

TEST(tl_audio_wav_reader, ReadSingleSampleFloat_RIFFWAV_PCM16) {
  TestCommonReadSingleSampleFloat(RIFFWAV_PCM16::kData,
                                  RIFFWAV_PCM16::kSamplesFloat,
                                  RIFFWAV_PCM16::kTolerance);
}

TEST(tl_audio_wav_reader, ReadSingleSampleFloat_RIFFWAV_FLOAT) {
  TestCommonReadSingleSampleFloat(RIFFWAV_FLOAT::kData,
                                  RIFFWAV_FLOAT::kSamplesFloat,
                                  RIFFWAV_FLOAT::kTolerance);
}

TEST(tl_audio_wav_reader, ReadSingleSampleFloat_RIFF_DOUBLE) {
  TestCommonReadSingleSampleFloat(RIFFWAV_DOUBLE::kData,
                                  RIFFWAV_DOUBLE::kSamplesFloat,
                                  RIFFWAV_DOUBLE::kTolerance);
}

TEST(tl_audio_wav_reader, ReadSingleSampleFloat_RF64WAV_PCM16) {
  TestCommonReadSingleSampleFloat(RF64WAV_PCM16::kData,
                                  RF64WAV_PCM16::kSamplesFloat,
                                  RF64WAV_PCM16::kTolerance);
}

TEST(tl_audio_wav_reader, ReadSingleSampleFloat_RF64WAV_EXTENSIBLE_PCM16) {
  TestCommonReadSingleSampleFloat(RF64WAV_EXTENSIBLE_PCM16::kData,
                                  RF64WAV_EXTENSIBLE_PCM16::kSamplesFloat,
                                  RF64WAV_EXTENSIBLE_PCM16::kTolerance);
}

TEST(tl_audio_wav_reader, ReadSingleSampleFloat_RF64WAV_EXTENSIBLE_FLOAT) {
  TestCommonReadSingleSampleFloat(RF64WAV_EXTENSIBLE_FLOAT::kData,
                                  RF64WAV_EXTENSIBLE_FLOAT::kSamplesFloat,
                                  RF64WAV_EXTENSIBLE_FLOAT::kTolerance);
}

TEST(tl_audio_wav_reader, ReadSingleSampleFloat_RF64WAV_EXTENSIBLE_DOUBLE) {
  TestCommonReadSingleSampleFloat(RF64WAV_EXTENSIBLE_DOUBLE::kData,
                                  RF64WAV_EXTENSIBLE_DOUBLE::kSamplesFloat,
                                  RF64WAV_EXTENSIBLE_DOUBLE::kTolerance);
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

TEST(tl_audio_wav_reader, ReadSingleSampleInt16_RIFFWAV_PCM16) {
  TestCommonReadSingleSampleInt16(RIFFWAV_PCM16::kData,
                                  RIFFWAV_PCM16::kSamplesInt16);
}

TEST(tl_audio_wav_reader, ReadSingleSampleInt16_RIFFWAV_FLOAT) {
  TestCommonReadSingleSampleInt16(RIFFWAV_FLOAT::kData,
                                  RIFFWAV_FLOAT::kSamplesInt16);
}

TEST(tl_audio_wav_reader, ReadSingleSampleInt16_RIFFWAV_DOUBLE) {
  TestCommonReadSingleSampleInt16(RIFFWAV_DOUBLE::kData,
                                  RIFFWAV_DOUBLE::kSamplesInt16);
}

TEST(tl_audio_wav_reader, ReadSingleSampleInt16_RF64WAV_PCM16) {
  TestCommonReadSingleSampleInt16(RF64WAV_PCM16::kData,
                                  RF64WAV_PCM16::kSamplesInt16);
}

TEST(tl_audio_wav_reader, ReadSingleSampleInt16_RF64WAV_EXTENSIBLE_PCM16) {
  TestCommonReadSingleSampleInt16(RF64WAV_EXTENSIBLE_PCM16::kData,
                                  RF64WAV_EXTENSIBLE_PCM16::kSamplesInt16);
}

TEST(tl_audio_wav_reader, ReadSingleSampleInt16_RF64WAV_EXTENSIBLE_FLOAT) {
  TestCommonReadSingleSampleInt16(RF64WAV_EXTENSIBLE_FLOAT::kData,
                                  RF64WAV_EXTENSIBLE_FLOAT::kSamplesInt16);
}

TEST(tl_audio_wav_reader, ReadSingleSampleInt16_RF64WAV_EXTENSIBLE_DOUBLE) {
  TestCommonReadSingleSampleInt16(RF64WAV_EXTENSIBLE_DOUBLE::kData,
                                  RF64WAV_EXTENSIBLE_DOUBLE::kSamplesInt16);
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

TEST(tl_audio_wav_reader, ReadAllSamplesFloat_RIFF_PCM) {
  TestCommonReadAllSamplesFloat(RIFFWAV_PCM16::kData,
                                RIFFWAV_PCM16::kSamplesFloat,
                                RIFFWAV_PCM16::kTolerance);
}

TEST(tl_audio_wav_reader, ReadAllSamplesFloat_RIFFWAV_FLOAT) {
  TestCommonReadAllSamplesFloat(RIFFWAV_FLOAT::kData,
                                RIFFWAV_FLOAT::kSamplesFloat,
                                RIFFWAV_FLOAT::kTolerance);
}

TEST(tl_audio_wav_reader, ReadAllSamplesFloat_RIFFWAV_DOUBLE) {
  TestCommonReadAllSamplesFloat(RIFFWAV_DOUBLE::kData,
                                RIFFWAV_DOUBLE::kSamplesFloat,
                                RIFFWAV_DOUBLE::kTolerance);
}

TEST(tl_audio_wav_reader, ReadAllSamplesFloat_RF64WAV_PCM16) {
  TestCommonReadAllSamplesFloat(RF64WAV_PCM16::kData,
                                RF64WAV_PCM16::kSamplesFloat,
                                RF64WAV_PCM16::kTolerance);
}

TEST(tl_audio_wav_reader, ReadAllSamplesFloat_RF64WAV_EXTENSIBLE_PCM16) {
  TestCommonReadAllSamplesFloat(RF64WAV_EXTENSIBLE_PCM16::kData,
                                RF64WAV_EXTENSIBLE_PCM16::kSamplesFloat,
                                RF64WAV_EXTENSIBLE_PCM16::kTolerance);
}

TEST(tl_audio_wav_reader, ReadAllSamplesFloat_RF64WAV_EXTENSIBLE_FLOAT) {
  TestCommonReadAllSamplesFloat(RF64WAV_EXTENSIBLE_FLOAT::kData,
                                RF64WAV_EXTENSIBLE_FLOAT::kSamplesFloat,
                                RF64WAV_EXTENSIBLE_FLOAT::kTolerance);
}

TEST(tl_audio_wav_reader, ReadAllSamplesFloat_RF64WAV_EXTENSIBLE_DOUBLE) {
  TestCommonReadAllSamplesFloat(RF64WAV_EXTENSIBLE_DOUBLE::kData,
                                RF64WAV_EXTENSIBLE_DOUBLE::kSamplesFloat,
                                RF64WAV_EXTENSIBLE_DOUBLE::kTolerance);
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

TEST(tl_audio_wav_reader, ReadAllSamplesInt16_RIFF_PCM) {
  TestCommonReadAllSamplesInt16(RIFFWAV_PCM16::kData,
                                RIFFWAV_PCM16::kSamplesInt16);
}

TEST(tl_audio_wav_reader, ReadAllSamplesInt16_RIFFWAV_FLOAT) {
  TestCommonReadAllSamplesInt16(RIFFWAV_FLOAT::kData,
                                RIFFWAV_FLOAT::kSamplesInt16);
}

TEST(tl_audio_wav_reader, ReadAllSamplesInt16_RIFFWAV_DOUBLE) {
  TestCommonReadAllSamplesInt16(RIFFWAV_DOUBLE::kData,
                                RIFFWAV_DOUBLE::kSamplesInt16);
}

TEST(tl_audio_wav_reader, ReadAllSamplesInt16_RF64WAV_PCM16) {
  TestCommonReadAllSamplesInt16(RF64WAV_PCM16::kData,
                                RF64WAV_PCM16::kSamplesInt16);
}

TEST(tl_audio_wav_reader, ReadAllSamplesInt16_RF64WAV_EXTENSIBLE_PCM16) {
  TestCommonReadAllSamplesInt16(RF64WAV_EXTENSIBLE_PCM16::kData,
                                RF64WAV_EXTENSIBLE_PCM16::kSamplesInt16);
}

TEST(tl_audio_wav_reader, ReadAllSamplesInt16_RF64WAV_EXTENSIBLE_FLOAT) {
  TestCommonReadAllSamplesInt16(RF64WAV_EXTENSIBLE_FLOAT::kData,
                                RF64WAV_EXTENSIBLE_FLOAT::kSamplesInt16);
}

TEST(tl_audio_wav_reader, ReadAllSamplesInt16_RF64WAV_EXTENSIBLE_DOUBLE) {
  TestCommonReadAllSamplesInt16(RF64WAV_EXTENSIBLE_DOUBLE::kData,
                                RF64WAV_EXTENSIBLE_DOUBLE::kSamplesInt16);
}

////////////////////////////////////////////////////////////////////////////////
// Simple pipeline.

TEST(tl_audio_wav_reader, SimplePipeline_lvalue) {
  FileReaderFromMemory file_reader(RIFFWAV_PCM16::kData);

  int sample_index = 0;
  const bool result = Reader<FileReaderFromMemory>::ReadAllSamples<int16_t, 2>(
      file_reader, [&](const std::span<const int16_t> sample) {
        EXPECT_LT(sample_index, RIFFWAV_PCM16::kSamplesInt16.size());
        EXPECT_THAT(
            sample,
            Pointwise(Eq(), RIFFWAV_PCM16::kSamplesInt16[sample_index]));
        ++sample_index;
      });

  EXPECT_TRUE(result);
  EXPECT_EQ(sample_index, 3);
}

TEST(tl_audio_wav_reader, SimplePipeline_rvalue) {
  int sample_index = 0;
  const bool result = Reader<FileReaderFromMemory>::ReadAllSamples<int16_t, 2>(
      FileReaderFromMemory(RIFFWAV_PCM16::kData),
      [&](const std::span<const int16_t> sample) {
        EXPECT_LT(sample_index, RIFFWAV_PCM16::kSamplesInt16.size());
        EXPECT_THAT(
            sample,
            Pointwise(Eq(), RIFFWAV_PCM16::kSamplesInt16[sample_index]));
        ++sample_index;
      });

  EXPECT_TRUE(result);
  EXPECT_EQ(sample_index, 3);
}

}  // namespace tiny_lib::audio_wav_reader
