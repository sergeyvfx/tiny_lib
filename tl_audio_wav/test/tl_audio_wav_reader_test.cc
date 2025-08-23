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

////////////////////////////////////////////////////////////////////////////////
// Byteswap().

TEST(Byteswap, Scalar) {
  EXPECT_EQ(internal::Byteswap(uint16_t(0x1234)), 0x3412);
  EXPECT_EQ(internal::Byteswap(uint32_t(0x12345678)), 0x78563412);
  EXPECT_EQ(internal::Byteswap(uint64_t(0x1234567890abcdef)),
            0xefcdab9078563412);
}

namespace {

// Common API of test data provider.
struct TestData {
  // Binary data of encoded file (as if it is saved on disk).
  virtual auto GetData() const -> std::span<const uint8_t> = 0;

  // Get depth of a single sample of a single channel.
  virtual auto GetBitDepth() const -> int = 0;

  // Tolerance when comparing samples read as floating point.
  virtual auto GetTolerance() const -> float = 0;

  // Expected values of samples when they are read as float.
  virtual auto GetSamplesFloat() const
      -> const std::span<const std::array<float, 2>> = 0;

  // Expected values of samples when they are read as int16_t.
  virtual auto GetSamplesInt16() const
      -> const std::span<const std::array<int16_t, 2>> = 0;
};

// Data of a simple WAV PCM file.
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
struct RIFFWAV_PCM : TestData {
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

  auto GetData() const -> std::span<const uint8_t> override { return kData; }
  auto GetBitDepth() const -> int override { return 16; }
  auto GetTolerance() const -> float override { return kTolerance; }
  auto GetSamplesFloat() const
      -> const std::span<const std::array<float, 2>> override {
    return kSamplesFloat;
  }
  auto GetSamplesInt16() const
      -> const std::span<const std::array<int16_t, 2>> override {
    return kSamplesInt16;
  }
};

// Data of a simple WAV FLOAT file.
// Stereo channel, 3 samples, 44100 samples per second.
//
// Can be generated with the following Python script:
//
//   import soundfile as sf
//   data = [(0.1, 0.4), (0.2, 0.5), (0.3, 0.6)]
//   sf.write("out.wav", data, 44100, format="WAV", subtype="FLOAT")
//
//  % xxd out.wav
//    00000000: 5249 4646 6800 0000 5741 5645 666d 7420  RIFFh...WAVEfmt
//    00000010: 1000 0000 0300 0200 44ac 0000 2062 0500  ........D... b..
//    00000020: 0800 2000 6661 6374 0400 0000 0300 0000  .. .fact........
//    00000030: 5045 414b 1800 0000 0100 0000 8cb4 a968  PEAK...........h
//    00000040: 9a99 993e 0200 0000 9a99 193f 0200 0000  ...>.......?....
//    00000050: 6461 7461 1800 0000 cdcc cc3d cdcc cc3e  data.......=...>
//    00000060: cdcc 4c3e 0000 003f 9a99 993e 9a99 193f  ..L>...?...>...?
struct RIFFWAV_FLOAT : TestData {
  static constexpr auto kData = std::to_array<uint8_t>({
      // clang-format off

      // RIFF chunk.
      'R', 'I' , 'F' , 'F',       // The RIFF chunk ID.
      0x68, 0x00, 0x00, 0x00,     // Chunk size.
      'W' , 'A' , 'V' , 'E',      // Format.

      // FMT chunk.
      'f', 'm' , 't' , ' ',       // The FMT chunk ID.
      0x10, 0x00, 0x00, 0x00,     // Chunk size (16 for FLOAT).
      0x03, 0x00,                 // Audio format (3 for FLOAT),
      0x02, 0x00,                 // Number of channels.
      0x44, 0xac, 0x00, 0x00,     // Sample rate (hex(44100) -> 0xac44).
      0x20, 0x62, 0x05, 0x00,     // Byte rate (hex(44100 * 2 * 4) -> 0x56220)
      0x08, 0x00,                 // Block align.
      0x20, 0x00,                 // Bits per sample.

      // fact chunk.
      'f', 'a', 'c', 't',
      0x04, 0x00, 0x00, 0x00,     // Chunk size (minimum 4).
      0x03, 0x00, 0x00, 0x00,     // The number of samples per channel.

      // PEAK chunk.
      'P', 'E', 'A', 'K',
      0x18, 0x00, 0x00, 0x00,     // Chunk size
      0x01, 0x00, 0x00, 0x00,     // Version.
      0x8c, 0xb4, 0x19, 0x68,     // Time stamp (seconds since 1/1/1970).
      // Peak for channel 1.
      0x9a, 0x99, 0x99, 0x3e,     // Signed value of peak.
      0x02, 0x00, 0x00, 0x00,     // The sample frame for the peak.
      // Peak for channel 2.
      0x9a, 0x99, 0x19, 0x3f,     // Signed value of peak.
      0x02, 0x00, 0x00, 0x00,     // The sample frame for the peak.

      // DATA chunk.
      'd' , 'a' , 't' , 'a' ,     // The DATA chunk ID.
      0x18, 0x00, 0x00, 0x00,     // Chunk size (24 = 2 * 3 * 32bit samples).

      // Samples.
      0xcd, 0xcc, 0xcc, 0x3d,  0xcd, 0xcc, 0xcc, 0x3e,  // Sample 1: (0.1, 0.4).
      0xcd, 0xcc, 0x4c, 0x3e,  0x00, 0x00, 0x00, 0x3f,  // Sample 2: (0.2, 0.5).
      0x9a, 0x99, 0x99, 0x3e,  0x9a, 0x99, 0x19, 0x3f,  // Sample 3: (0.3, 0.6).
      // clang-format on
  });

  // Tolerance when comparing samples read as floating point.
  // TODO(sergey): Could actually be an exact comparison?
  static constexpr float kTolerance = 1e-12f;

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

  auto GetData() const -> std::span<const uint8_t> override { return kData; }
  auto GetBitDepth() const -> int override { return 32; }
  auto GetTolerance() const -> float override { return kTolerance; };
  auto GetSamplesFloat() const
      -> const std::span<const std::array<float, 2>> override {
    return kSamplesFloat;
  };
  auto GetSamplesInt16() const
      -> const std::span<const std::array<int16_t, 2>> override {
    return kSamplesInt16;
  }
};

// Data of a simple WAV DOUBLE file.
// Stereo channel, 3 samples, 44100 samples per second.
//
// Can be generated with the following Python script:
//
//   import soundfile as sf
//   data = [(0.1, 0.4), (0.2, 0.5), (0.3, 0.6)]
//   sf.write("out.wav", data, 44100, format="WAV", subtype="DOUBLE")
//
//  % xxd out.wav
//    00000000: 5249 4646 8000 0000 5741 5645 666d 7420  RIFF....WAVEfmt
//    00000010: 1000 0000 0300 0200 44ac 0000 40c4 0a00  ........D...@...
//    00000020: 1000 4000 6661 6374 0400 0000 0300 0000  ..@.fact........
//    00000030: 5045 414b 1800 0000 0100 0000 31e4 a968  PEAK........1..h
//    00000040: 9a99 993e 0200 0000 9a99 193f 0200 0000  ...>.......?....
//    00000050: 6461 7461 3000 0000 9a99 9999 9999 b93f  data0..........?
//    00000060: 9a99 9999 9999 d93f 9a99 9999 9999 c93f  .......?.......?
//    00000070: 0000 0000 0000 e03f 3333 3333 3333 d33f  .......?333333.?
//    00000080: 3333 3333 3333 e33f                      333333.?
struct RIFFWAV_DOUBLE : TestData {
  static constexpr auto kData = std::to_array<uint8_t>({
      // clang-format off

      // RIFF chunk.
      'R', 'I' , 'F' , 'F',       // The RIFF chunk ID.
      0x80, 0x00, 0x00, 0x00,     // Chunk size.
      'W' , 'A' , 'V' , 'E',      // Format.

      // FMT chunk.
      'f', 'm' , 't' , ' ',       // The FMT chunk ID.
      0x10, 0x00, 0x00, 0x00,     // Chunk size (16 for FLOAT).
      0x03, 0x00,                 // Audio format (3 for FLOAT),
      0x02, 0x00,                 // Number of channels.
      0x44, 0xac, 0x00, 0x00,     // Sample rate (hex(44100) -> 0xac44).
      0x40, 0xc4, 0x0a, 0x00,     // Byte rate (hex(44100 * 2 * 4) -> 0x56220)
      0x10, 0x00,                 // Block align.
      0x40, 0x00,                 // Bits per sample.

      // fact chunk.
      'f', 'a', 'c', 't',
      0x04, 0x00, 0x00, 0x00,     // Chunk size (minimum 4).
      0x03, 0x00, 0x00, 0x00,     // The number of samples per channel.

      // PEAK chunk.
      'P', 'E', 'A', 'K',
      0x18, 0x00, 0x00, 0x00,     // Chunk size
      0x01, 0x00, 0x00, 0x00,     // Version.
      0x31, 0xe4, 0xa9, 0x68,     // Time stamp (seconds since 1/1/1970).
      // Peak for channel 1.
      0x9a, 0x99, 0x99, 0x3e,     // Signed value of peak.
      0x02, 0x00, 0x00, 0x00,     // The sample frame for the peak.
      // Peak for channel 2.
      0x9a, 0x99, 0x19, 0x3f,     // Signed value of peak.
      0x02, 0x00, 0x00, 0x00,     // The sample frame for the peak.

      // DATA chunk.
      'd' , 'a' , 't' , 'a' ,     // The DATA chunk ID.
      0x30, 0x00, 0x00, 0x00,     // Chunk size (48 = 2 * 3 * 64bit samples).

      // Samples.
      0x9a, 0x99, 0x99, 0x99,  0x99, 0x99, 0xb9, 0x3f,  // Sample 1: (0.1,
      0x9a, 0x99, 0x99, 0x99,  0x99, 0x99, 0xd9, 0x3f,  //            0.4).
      0x9a, 0x99, 0x99, 0x99,  0x99, 0x99, 0xc9, 0x3f,  // Sample 2: (0.2,
      0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0xe0, 0x3f,  //            0.5).
      0x33, 0x33, 0x33, 0x33,  0x33, 0x33, 0xd3, 0x3f,  // Sample 3: (0.3,
      0x33, 0x33, 0x33, 0x33,  0x33, 0x33, 0xe3, 0x3f,  //            0.6).
      // clang-format on
  });

  // Tolerance when comparing samples read as floating point.
  static constexpr float kTolerance = 1e-12f;

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

  auto GetData() const -> std::span<const uint8_t> override { return kData; }
  auto GetBitDepth() const -> int override { return 64; }
  auto GetTolerance() const -> float override { return kTolerance; };
  auto GetSamplesFloat() const
      -> const std::span<const std::array<float, 2>> override {
    return kSamplesFloat;
  };
  auto GetSamplesInt16() const
      -> const std::span<const std::array<int16_t, 2>> override {
    return kSamplesInt16;
  }
};

// Data of WAV file which uses RF64 PCM format.
// Stereo channel, 3 samples, 44100 samples per second.
//
// Used by some SDR software like SDR#.
struct RF64WAV_PCM16 : TestData {
  static constexpr auto kData = std::to_array<uint8_t>({
      // clang-format off

      // RIFF chunk.
      'R', 'F' , '6' , '4',       // The RIFF chunk ID.
      0xff, 0xff, 0xff, 0xff,     // Chunk size.
      'W' , 'A' , 'V' , 'E',      // Format.

      // ds64 chunk.
      'd', 's', '6', '4',         // The ds64 chunk ID.
      0x1c, 0x00, 0x00, 0x00,     // Chunk size.
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

  auto GetData() const -> std::span<const uint8_t> override { return kData; }
  auto GetBitDepth() const -> int override { return 16; }
  auto GetTolerance() const -> float override { return kTolerance; };
  auto GetSamplesFloat() const
      -> const std::span<const std::array<float, 2>> override {
    return kSamplesFloat;
  };
  auto GetSamplesInt16() const
      -> const std::span<const std::array<int16_t, 2>> override {
    return kSamplesInt16;
  }
};

// Data of WAV file which uses RF64 Extensible PCM_ format.
// Stereo channel, 3 samples, 44100 samples per second.
//
// Can be generated with the following Python script:
//
//   import soundfile as sf
//   data = [(0.1, 0.4), (0.2, 0.5), (0.3, 0.6)]
//   sf.write("out.wav", data, 44100, format="RF64", subtype="PCM_16")
//
//  % xxd out.wav
//    00000000: 5246 3634 ffff ffff 5741 5645 6473 3634  RF64....WAVEds64
//    00000010: 1c00 0000 6c00 0000 0000 0000 0c00 0000  ....l...........
//    00000020: 0000 0000 0300 0000 0000 0000 0000 0000  ................
//    00000030: 666d 7420 2800 0000 feff 0200 44ac 0000  fmt (.......D...
//    00000040: 10b1 0200 0400 1000 1600 1000 0300 0000  ................
//    00000050: 0100 0000 0000 1000 8000 00aa 0038 9b71  .............8.q
//    00000060: 6461 7461 ffff ffff cc0c 3333 9919 0040  data......33...@
//    00000070: 6626 cc4c                                f&.L
struct RF64WAV_EXTENSIBLE_PCM16 : TestData {
  static constexpr auto kData = std::to_array<uint8_t>({
      // clang-format off

      // RIFF chunk.
      'R', 'F' , '6' , '4',       // The RIFF chunk ID.
      0xff, 0xff, 0xff, 0xff,     // Chunk size.
      'W' , 'A' , 'V' , 'E',      // Format.

      // ds64 chunk.
      'd', 's', '6', '4',         // The ds64 chunk ID.
      0x1c, 0x00, 0x00, 0x00,     // Chunk size.
      0x6c, 0x00, 0x00, 0x00,     // Low 4 byte size of RF64 block.
      0x00, 0x00, 0x00, 0x00,     // High 4 byte size of RF64 block.
      0x0c, 0x00, 0x00, 0x00,     // Low 4 byte size of data chunk.
      0x00, 0x00, 0x00, 0x00,     // High 4 byte size of data chunk.
      0x03, 0x00, 0x00, 0x00,     // Low 4 byte sample count of fact chunk.
      0x00, 0x00, 0x00, 0x00,     // High 4 byte sample count of fact chunk.
      0x00, 0x00, 0x00, 0x00,     // Number of valid entries in array “table”.

      // FMT chunk.
      'f', 'm' , 't' , ' ',       // The FMT chunk ID.
      0x28, 0x00, 0x00, 0x00,     // Chunk size (40 for PCM).
      0xfe, 0xff,                 // Audio format (0xFFFE for EXTENSIBLE),
      0x02, 0x00,                 // Number of channels.
      0x44, 0xac, 0x00, 0x00,     // Sample rate (hex(44100) -> 0xac44).
      0x10, 0xb1, 0x02, 0x00,     // Byte rate (hex(44100 * 2 * 2) -> 0x2b110)
      0x04, 0x00,                 // Block align.
      0x10, 0x00,                 // Bits per sample.
      0x16, 0x00,                 // Extra information (after cbSize) to store.
      0x10, 0x00,                 // Valid bits per sample.
      0x03, 0x00, 0x00, 0x00,     // Channel mask for channel allocation.
      // Guid (uin32_t, uint16_t, uint16_t, uint32_t, uint32_t)
                                  // KSDATAFORMAT_SUBTYPE_PCM
      0x01, 0x00, 0x00, 0x00,     // data1 = 0x00000001
      0x00, 0x00,                 // data2 = 0x0000
      0x10, 0x00,                 // data3 = 0x0010
      0x80, 0x00, 0x00, 0xaa,     // data4 = 0xAA000080
      0x00, 0x38, 0x9b, 0x71,     // data5 = 0x719B3800

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

  auto GetData() const -> std::span<const uint8_t> override { return kData; }
  auto GetBitDepth() const -> int override { return 16; }
  auto GetTolerance() const -> float override { return kTolerance; };
  auto GetSamplesFloat() const
      -> const std::span<const std::array<float, 2>> override {
    return kSamplesFloat;
  };
  auto GetSamplesInt16() const
      -> const std::span<const std::array<int16_t, 2>> override {
    return kSamplesInt16;
  }
};

// Data of WAV file which uses RF64 Extensible FLOAT format.
// Stereo channel, 3 samples, 44100 samples per second.
//
// Can be generated with the following Python script:
//
//   import soundfile as sf
//   data = [(0.1, 0.4), (0.2, 0.5), (0.3, 0.6)]
//   sf.write("out.wav", data, 44100, format="RF64", subtype="FLOAT")
//
//  % xxd out.wav
//    00000000: 5246 3634 ffff ffff 5741 5645 6473 3634  RF64....WAVEds64
//    00000010: 1c00 0000 7800 0000 0000 0000 1800 0000  ....x...........
//    00000020: 0000 0000 0300 0000 0000 0000 0000 0000  ................
//    00000030: 666d 7420 2800 0000 feff 0200 44ac 0000  fmt (.......D...
//    00000040: 2062 0500 0800 2000 1600 2000 0300 0000   b.... ... .....
//    00000050: 0300 0000 0000 1000 8000 00aa 0038 9b71  .............8.q
//    00000060: 6461 7461 ffff ffff cdcc cc3d cdcc cc3e  data.......=...>
//    00000070: cdcc 4c3e 0000 003f 9a99 993e 9a99 193f  ..L>...?...>...?
struct RF64WAV_EXTENSIBLE_FLOAT : TestData {
  static constexpr auto kData = std::to_array<uint8_t>({
      // clang-format off

      // RIFF chunk.
      'R', 'F' , '6' , '4',       // The RIFF chunk ID.
      0xff, 0xff, 0xff, 0xff,     // Chunk size.
      'W' , 'A' , 'V' , 'E',      // Format.

      // ds64 chunk.
      'd', 's', '6', '4',         // The ds64 chunk ID.
      0x1c, 0x00, 0x00, 0x00,     // Chunk size.
      0x78, 0x00, 0x00, 0x00,     // Low 4 byte size of RF64 block.
      0x00, 0x00, 0x00, 0x00,     // High 4 byte size of RF64 block.
      0x18, 0x00, 0x00, 0x00,     // Low 4 byte size of data chunk.
      0x00, 0x00, 0x00, 0x00,     // High 4 byte size of data chunk.
      0x03, 0x00, 0x00, 0x00,     // Low 4 byte sample count of fact chunk.
      0x00, 0x00, 0x00, 0x00,     // High 4 byte sample count of fact chunk.
      0x00, 0x00, 0x00, 0x00,     // Number of valid entries in array “table”.

      // FMT chunk.
      'f', 'm' , 't' , ' ',       // The FMT chunk ID.
      0x28, 0x00, 0x00, 0x00,     // Chunk size (40 for FLOAT).
      0xfe, 0xff,                 // Audio format (0xFFFE for EXTENSIBLE),
      0x02, 0x00,                 // Number of channels.
      0x44, 0xac, 0x00, 0x00,     // Sample rate (hex(44100) -> 0xac44).
      0x20, 0x62, 0x05, 0x00,     // Byte rate (hex(44100 * 2 * 4) -> 0x56220)
      0x08, 0x00,                 // Block align.
      0x20, 0x00,                 // Bits per sample.
      0x16, 0x00,                 // Extra information (after cbSize) to store.
      0x20, 0x00,                 // Valid bits per sample.
      0x03, 0x00, 0x00, 0x00,     // Channel mask for channel allocation.
      // Guid (uin32_t, uint16_t, uint16_t, uint32_t, uint32_t)
                                  // KSDATAFORMAT_SUBTYPE_FLOAT
      0x03, 0x00, 0x00, 0x00,     // data1 = 0x00000003
      0x00, 0x00,                 // data2 = 0x0000
      0x10, 0x00,                 // data3 = 0x0010
      0x80, 0x00, 0x00, 0xaa,     // data4 = 0xAA000080
      0x00, 0x38, 0x9b, 0x71,     // data5 = 0x719B3800

      // DATA chunk.
      'd' , 'a' , 't' , 'a' ,     // The DATA chunk ID.
      0xff, 0xff, 0xff, 0xff,     // Chunk size (12 = 2 * 3 * 16bit samples).

      // Samples.
      0xcd, 0xcc, 0xcc, 0x3d,  0xcd, 0xcc, 0xcc, 0x3e,  // Sample 1: (0.1, 0.4).
      0xcd, 0xcc, 0x4c, 0x3e,  0x00, 0x00, 0x00, 0x3f,  // Sample 2: (0.2, 0.5).
      0x9a, 0x99, 0x99, 0x3e,  0x9a, 0x99, 0x19, 0x3f,  // Sample 3: (0.3, 0.6).
      // clang-format on
  });

  // Tolerance when comparing samples read as floating point.
  // TODO(sergey): Could actually be an exact comparison?
  static constexpr float kTolerance = 1e-12f;

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

  auto GetData() const -> std::span<const uint8_t> override { return kData; }
  auto GetBitDepth() const -> int override { return 32; }
  auto GetTolerance() const -> float override { return kTolerance; };
  auto GetSamplesFloat() const
      -> const std::span<const std::array<float, 2>> override {
    return kSamplesFloat;
  };
  auto GetSamplesInt16() const
      -> const std::span<const std::array<int16_t, 2>> override {
    return kSamplesInt16;
  }
};

// Data of WAV file which uses RF64 Extensible DOUBLE format.
// Stereo channel, 3 samples, 44100 samples per second.
//
// Can be generated with the following Python script:
//
//   import soundfile as sf
//   data = [(0.1, 0.4), (0.2, 0.5), (0.3, 0.6)]
//   sf.write("out.wav", data, 44100, format="RF64", subtype="DOUBLE")
//
//  % xxd out.wav
//    00000000: 5246 3634 ffff ffff 5741 5645 6473 3634  RF64....WAVEds64
//    00000010: 1c00 0000 9000 0000 0000 0000 3000 0000  ............0...
//    00000020: 0000 0000 0300 0000 0000 0000 0000 0000  ................
//    00000030: 666d 7420 2800 0000 feff 0200 44ac 0000  fmt (.......D...
//    00000040: 40c4 0a00 1000 4000 1600 4000 0300 0000  @.....@...@.....
//    00000050: 0300 0000 0000 1000 8000 00aa 0038 9b71  .............8.q
//    00000060: 6461 7461 ffff ffff 9a99 9999 9999 b93f  data...........?
//    00000070: 9a99 9999 9999 d93f 9a99 9999 9999 c93f  .......?.......?
//    00000080: 0000 0000 0000 e03f 3333 3333 3333 d33f  .......?333333.?
//    00000090: 3333 3333 3333 e33f                      333333.?
struct RF64WAV_EXTENSIBLE_DOUBLE : TestData {
  static constexpr auto kData = std::to_array<uint8_t>({
      // clang-format off

      // RIFF chunk.
      'R', 'F' , '6' , '4',       // The RIFF chunk ID.
      0xff, 0xff, 0xff, 0xff,     // Chunk size.
      'W' , 'A' , 'V' , 'E',      // Format.

      // ds64 chunk.
      'd', 's', '6', '4',         // The ds64 chunk ID.
      0x1c, 0x00, 0x00, 0x00,     // Chunk size.
      0x90, 0x00, 0x00, 0x00,     // Low 4 byte size of RF64 block.
      0x00, 0x00, 0x00, 0x00,     // High 4 byte size of RF64 block.
      0x30, 0x00, 0x00, 0x00,     // Low 4 byte size of data chunk.
      0x00, 0x00, 0x00, 0x00,     // High 4 byte size of data chunk.
      0x03, 0x00, 0x00, 0x00,     // Low 4 byte sample count of fact chunk.
      0x00, 0x00, 0x00, 0x00,     // High 4 byte sample count of fact chunk.
      0x00, 0x00, 0x00, 0x00,     // Number of valid entries in array “table”.

      // FMT chunk.
      'f', 'm' , 't' , ' ',       // The FMT chunk ID.
      0x28, 0x00, 0x00, 0x00,     // Chunk size (40 for FLOAT).
      0xfe, 0xff,                 // Audio format (0xFFFE for EXTENSIBLE),
      0x02, 0x00,                 // Number of channels.
      0x44, 0xac, 0x00, 0x00,     // Sample rate (hex(44100) -> 0xac44).
      0x40, 0xc4, 0x0a, 0x00,     // Byte rate (hex(44100 * 2 * 8) -> 0xac440)
      0x10, 0x00,                 // Block align.
      0x40, 0x00,                 // Bits per sample.
      0x16, 0x00,                 // Extra information (after cbSize) to store.
      0x40, 0x00,                 // Valid bits per sample.
      0x03, 0x00, 0x00, 0x00,     // Channel mask for channel allocation.
      // Guid (uin32_t, uint16_t, uint16_t, uint32_t, uint32_t)
                                  // KSDATAFORMAT_SUBTYPE_FLOAT
      0x03, 0x00, 0x00, 0x00,     // data1 = 0x00000003
      0x00, 0x00,                 // data2 = 0x0000
      0x10, 0x00,                 // data3 = 0x0010
      0x80, 0x00, 0x00, 0xaa,     // data4 = 0xAA000080
      0x00, 0x38, 0x9b, 0x71,     // data5 = 0x719B3800

      // DATA chunk.
      'd' , 'a' , 't' , 'a' ,     // The DATA chunk ID.
      0xff, 0xff, 0xff, 0xff,     // Chunk size (12 = 2 * 3 * 16bit samples).

      // Samples.
      0x9a, 0x99, 0x99, 0x99,  0x99, 0x99, 0xb9, 0x3f,  // Sample 1: (0.1,
      0x9a, 0x99, 0x99, 0x99,  0x99, 0x99, 0xd9, 0x3f,  //            0.4).
      0x9a, 0x99, 0x99, 0x99,  0x99, 0x99, 0xc9, 0x3f,  // Sample 2: (0.2,
      0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0xe0, 0x3f,  //            0.5).
      0x33, 0x33, 0x33, 0x33,  0x33, 0x33, 0xd3, 0x3f,  // Sample 3: (0.3,
      0x33, 0x33, 0x33, 0x33,  0x33, 0x33, 0xe3, 0x3f,  //            0.6).
      // clang-format on
  });

  // Tolerance when comparing samples read as floating point.
  // TODO(sergey): Could actually be an exact comparison?
  static constexpr float kTolerance = 1e-12f;

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

  auto GetData() const -> std::span<const uint8_t> override { return kData; }
  auto GetBitDepth() const -> int override { return 64; }
  auto GetTolerance() const -> float override { return kTolerance; };
  auto GetSamplesFloat() const
      -> const std::span<const std::array<float, 2>> override {
    return kSamplesFloat;
  };
  auto GetSamplesInt16() const
      -> const std::span<const std::array<int16_t, 2>> override {
    return kSamplesInt16;
  }
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

static void TestCommonFormatSpec(const TestData& test_data) {
  FileReaderFromMemory file_reader(test_data.GetData());
  Reader<FileReaderFromMemory> wav_reader;

  EXPECT_TRUE(wav_reader.Open(file_reader));

  const FormatSpec format_spec = wav_reader.GetFormatSpec();

  EXPECT_EQ(format_spec.num_channels, 2);
  EXPECT_EQ(format_spec.sample_rate, 44100);
  EXPECT_EQ(format_spec.bit_depth, test_data.GetBitDepth());
}

TEST(tl_audio_wav_reader, FormatSpec_RIFFWAV_PCM) {
  TestCommonFormatSpec(RIFFWAV_PCM());
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

TEST(tl_audio_wav_reader, GetDurationInSeconds_RIFFWAV_PCM) {
  TestCommonGetDurationInSeconds(RIFFWAV_PCM::kData);
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

TEST(tl_audio_wav_reader, ReadSingleSampleFloat_RIFFWAV_PCM) {
  TestCommonReadSingleSampleFloat(
      RIFFWAV_PCM::kData, RIFFWAV_PCM::kSamplesFloat, RIFFWAV_PCM::kTolerance);
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

TEST(tl_audio_wav_reader, ReadSingleSampleInt16_RIFFWAV_PCM) {
  TestCommonReadSingleSampleInt16(RIFFWAV_PCM::kData,
                                  RIFFWAV_PCM::kSamplesInt16);
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
  TestCommonReadAllSamplesFloat(
      RIFFWAV_PCM::kData, RIFFWAV_PCM::kSamplesFloat, RIFFWAV_PCM::kTolerance);
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
  TestCommonReadAllSamplesInt16(RIFFWAV_PCM::kData, RIFFWAV_PCM::kSamplesInt16);
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
  FileReaderFromMemory file_reader(RIFFWAV_PCM::kData);

  int sample_index = 0;
  const bool result = Reader<FileReaderFromMemory>::ReadAllSamples<int16_t, 2>(
      file_reader, [&](const std::span<const int16_t> sample) {
        EXPECT_LT(sample_index, RIFFWAV_PCM::kSamplesInt16.size());
        EXPECT_THAT(sample,
                    Pointwise(Eq(), RIFFWAV_PCM::kSamplesInt16[sample_index]));
        ++sample_index;
      });

  EXPECT_TRUE(result);
  EXPECT_EQ(sample_index, 3);
}

TEST(tl_audio_wav_reader, SimplePipeline_rvalue) {
  int sample_index = 0;
  const bool result = Reader<FileReaderFromMemory>::ReadAllSamples<int16_t, 2>(
      FileReaderFromMemory(RIFFWAV_PCM::kData),
      [&](const std::span<const int16_t> sample) {
        EXPECT_LT(sample_index, RIFFWAV_PCM::kSamplesInt16.size());
        EXPECT_THAT(sample,
                    Pointwise(Eq(), RIFFWAV_PCM::kSamplesInt16[sample_index]));
        ++sample_index;
      });

  EXPECT_TRUE(result);
  EXPECT_EQ(sample_index, 3);
}

}  // namespace tiny_lib::audio_wav_reader
