// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

// Reader of WAVE files.
//
// The reader implements streamed and polled reading of a WAVE file using given
// file reader provider for data access. The file reader is provided as a
// template argument which allows for the most optimal code generation, avoiding
// per-read-call function indirections in the cost of possible bigger binary
// size.
//
//
// Example
// =======
//
//   // Streamed reading.
//
//   MyFileReader file_reader;
//   Reader<MyFileReader> wav_reader;
//
//   // Open WAVE file for reading.
//   if (!wav_reader.Open(file_reader)) {
//     std::cerr << "Error opening WAVE file" << std::endl;
//   }
//
//   // Read samples from file and print them.
//   wav_reader.ReadAllSamples<float, 2>(
//       [&](const std::span<const float> sample) {
//         for (const float value : sample) {
//           std::cout << value << " ";
//         }
//         std::cout << std::endl;
//       });
//
//
//   // Polled reading.
//
//   MyFIleReader file_reader;
//   Reader<MyFileReader> wav_reader(file_reader);
//
//   std::array<float, 2> sample;
//   while (wav_reader.ReadSingleSample<float>(sample)) {
//     for (const float value : sample) {
//       std::cout << value << " ";
//     }
//     std::cout << std::endl;
//   }
//
// File Reader
// ===========
//
// The file reader is to implement a Read() method which can be invoked with the
// following signature:
//
//   auto Read(void* ptr, SizeType num_bytes_to_read) -> IntType;
//
// It is possible to have trailing arguments with default values in the
// method if they do not change the expected invoke arguments.
//
// The num_bytes_to_read denotes the number of bytes which the WAV reader
// expects to be read from the file.
//
// Upon success the Read() is to return the number of bytes actually read.
//
// The WAV reader expects the number of actually read bytes to match the number
// of requested bytes to read. This gives flexibility to the error handling in
// the file reader implementation: both read() and fread() style of the return
// value in the case of error or reading past the EOF will work for the WAV
// reader.
//
//
// Limitations
// ===========
//
// - Only uncompressed PCM 16-bit signed integer data type is supported.
//
// - Streaming is not optimized: it can take substantial time to read int16 file
//   as float samples. There are optimizations that use buffered operations, but
//   it could still benefit from vectorization.
//
// - Reading RF64 files is only supported for file does not use tables.
//
//
// Version history
// ===============
//
//   0.0.3-alpha    (14 Dec 2024)    Support RF64 file format.
//   0.0.2-alpha    (13 Dec 2024)    Various improvements with the goal to
//                                   support buffered reading:
//                                   - Implement buffered reading.
//                                   - [[nodiscard]] is used for internal
//                                     functions.
//   0.0.1-alpha    (28 Dec 2023)    First public release.

#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>

#include "tl_audio_wav_writer.h"

// Semantic version of the tl_audio_wav_reader library.
#define TL_AUDIO_WAV_READER_VERSION_MAJOR 0
#define TL_AUDIO_WAV_READER_VERSION_MINOR 0
#define TL_AUDIO_WAV_READER_VERSION_REVISION 1

// Namespace of the module.
// The outer name spaces which surrounds the ABI-version namespace.
#ifndef TL_AUDIO_WAV_READER_NAMESPACE
#  define TL_AUDIO_WAV_READER_NAMESPACE tiny_lib::audio_wav_reader
#endif

// The size of the buffer used for single frame-sample.
//
// It is used, for example, in ReadSingleSample() to optimize disk and memory
// access pattern. Measured in the number of per-channel samples. The default
// value covers the case of surround files with center channel.
//
// Must be non-negative.
// Value of 0 or 1 disables buffered reading, which leads to more poor
// performance but uses less stack memory.
#ifndef TL_AUDIO_WAV_READER_SAMPLE_FRAME_BUFFER_SIZE
#  define TL_AUDIO_WAV_READER_SAMPLE_FRAME_BUFFER_SIZE 5
#endif

// The size of the buffer used for storing multiple frame-samples.
//
// It is used, for example, in ReadAllSamples() to optimize disk and memory
// access pattern. Measured in the number of per-channel samples.
//
// Must be non-negative.
// Value of 0 or 1 disables buffered reading, which leads to more poor
// performance but uses less stack memory.
#ifndef TL_AUDIO_WAV_READER_BUFFER_SIZE
#  define TL_AUDIO_WAV_READER_BUFFER_SIZE                                      \
    (TL_AUDIO_WAV_READER_SAMPLE_FRAME_BUFFER_SIZE * 32)
#endif

// Helpers for TL_AUDIO_WAV_READER_VERSION_NAMESPACE.
//
// Typical extra indirection for such conversion to allow macro to be expanded
// before it is converted to string.
#define TL_AUDIO_WAV_READER_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)     \
  v_##id1##_##id2##_##id3
#define TL_AUDIO_WAV_READER_VERSION_NAMESPACE_CONCAT(id1, id2, id3)            \
  TL_AUDIO_WAV_READER_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)

// Constructs identifier suitable for namespace denoting the current library
// version.
//
// For example: TL_AUDIO_WAV_READER_VERSION_NAMESPACE -> v_0_1_9
#define TL_AUDIO_WAV_READER_VERSION_NAMESPACE                                  \
  TL_AUDIO_WAV_READER_VERSION_NAMESPACE_CONCAT(                                \
      TL_AUDIO_WAV_READER_VERSION_MAJOR,                                       \
      TL_AUDIO_WAV_READER_VERSION_MINOR,                                       \
      TL_AUDIO_WAV_READER_VERSION_REVISION)

// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace TL_AUDIO_WAV_READER_NAMESPACE {
inline namespace TL_AUDIO_WAV_READER_VERSION_NAMESPACE {

////////////////////////////////////////////////////////////////////////////////
// Forward declaration of internal types.

namespace internal {

enum class ChunkID : uint32_t;
enum class Format : uint32_t;
enum class AudioFormat : uint16_t;

struct ChunkHeader;
struct RIFFData;
struct DS64;
struct FormatData;

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
// Public declarations.

// Information about format in which the audio data is stored in a WAV file.
struct FormatSpec {
  // Number of channels.
  // Rather self-explanatory: 1 is mono, 2 is stereo, 5 is surround.
  int num_channels{-1};

  // Sample rate in samples per second.
  int sample_rate{-1};

  // Bit depth for per-channel sample values.
  // For example: 16 means values are stored as 16-bit signed integers.
  int bit_depth{-1};
};

// The WAV file reader declaration.
template <class FileReader>
class Reader {
 public:
  // Constructs a new WAV file reader.
  Reader() = default;

  Reader(Reader&& other) noexcept = default;
  auto operator=(Reader&& other) -> Reader& = default;

  // Delete copy constructor and assignment operator.
  //
  // Simplifies the FileReader API by avoiding requirement to implement the
  // copy file descriptor API call.
  Reader(const Reader& other) noexcept = delete;
  auto operator=(const Reader& other) -> Reader& = delete;

  ~Reader() = default;

  // Open the file for reading using given file reader.
  //
  // The file reader is referenced by the WAV reader and caller is to guarantee
  // it is valid throughout the WAV file open and samples reading.
  //
  // Performs check for WAV header and verifies that the sample data can be read
  // from the file: that the samples are stored in a supported format.
  //
  // The underlying file reader is supposed to be open and ready to be read, as
  // well as positioned at the beginning of the WAV file.
  //
  // Returns true if audio samples are ready to be read.
  //
  // Multiple calls to Open() is undefined.
  auto Open(FileReader& file_reader) -> bool;

  // Get format specification of the file.
  //
  // Requires WAV file to be successfully open for read first and will have an
  // undefined behavior if the Open() returned false.
  inline auto GetFormatSpec() const -> const FormatSpec&;

  // File duration in seconds.
  inline auto GetDurationInSeconds() const -> float;

  // Read single sample which consists of values of all channels.
  //
  // For example, read a single sample of both left and right channels of a
  // stereo WAV file into the given buffer.
  //
  // At most sample.size() channels are written to the sample buffer, the rest
  // of channels of the current sample are ignored.
  //
  // If the number of channels in the file is less than samples.size() then only
  // channels which present in the file are written to the sample buffer.
  //
  // The reader is expected to be successfully open. Calling this function on
  // a non-open reader is undefined.
  //
  // The sample data type is converted from the type-on-disk to the ValueType.
  //
  // Returns true if sample is successfully read.
  template <class ValueType>
  auto ReadSingleSample(std::span<ValueType> sample) -> bool;

  // Read all samples from the file, invoking the given callback for each of the
  // read samples.
  //
  // The callback will receive the given list of arguments, followed by a span
  // which contains per-channel values of a sample.
  //
  // At maximum MaxChannels values is provided to the callback.
  //
  // Returns true if all samples were successfully read.
  template <class ValueType, size_t MaxChannels, class F, class... Args>
  auto ReadAllSamples(F&& callback, Args&&... args) -> bool;

  // Read all audio samples from the file.
  //
  // This is a shortcut to creation of the Reader object, opening it, reading
  // all samples.
  template <class ValueType,
            size_t MaxChannels,
            class FileReaderType,
            class F,
            class... Args>
  static auto ReadAllSamples(FileReaderType&& file_reader,
                             F&& callback,
                             Args&&... args) -> bool;

 private:
  // Read the header of the WAV file and store it in the internal state.
  [[nodiscard]] inline auto ReadHeader() -> bool;

  // Convert value from how they were read from file to the native endianess.
  template <class T>
  inline void FileToNativeEndian(T& value);

  // Read object from file stream directly into memory, without endian
  // conversion or any other semantic checks.
  //
  // Returns true if the object is fully read, false otherwise.
  template <class T>
  [[nodiscard]] inline auto ReadObjectInMemory(T& object) -> bool;

  // Rad chunk header from the file
  [[nodiscard]] inline auto ReadChunkHeader(internal::ChunkHeader& header)
      -> bool;

  // Read the RIFF/RIFX/RF64 header.
  // Upon success will initialize the file endian.
  [[nodiscard]] inline auto ReadRIFFHeader(internal::ChunkHeader& riff_header)
      -> bool;

  // Read the RIFF/RIFX data.
  [[nodiscard]] inline auto ReadRIFFData(internal::RIFFData& riff_data) -> bool;

  // Skip given number of bytes from the current location of file.
  // Returns true if all requested bytes has been skipped.
  [[nodiscard]] inline auto FileSkipNumBytes(uint64_t num_bytes) -> bool;

  // Read the file seeking for a chunk header of given ID.
  //
  // If the chunk is found the reader is positioned right after it and true
  // is returned.
  [[nodiscard]] inline auto SeekChunkID(internal::ChunkID id,
                                        internal::ChunkHeader& header) -> bool;

  // Read the DS64 data.
  [[nodiscard]] inline auto ReadDS64(internal::DS64& ds64) -> bool;

  // Seek for a format chunk and read its data.
  [[nodiscard]] inline auto SeekAndReadFormatData(
      internal::FormatData& format_data) -> bool;

  // Internal implementation of the public ReadSingleSample().
  //
  // Uses a value type used for storage as a template argument.
  template <class ValueTypeInFile, class ValueTypeInBuffer>
  [[nodiscard]] auto ReadSingleSample(std::span<ValueTypeInBuffer> sample)
      -> bool;

  // Internal implementation of the private ReadSingleSample() which reads
  // samples one by one directly from the disk. It is not very performant but
  // uses the least amount of stack size.
  // Expects that the sample is sized accordingly to the requested number of
  // samples and clamped to the actual spec number of samples. Does not perform
  // skip of unused channels.
  template <class ValueTypeInFile, class ValueTypeInBuffer>
  [[nodiscard]] auto ReadSingleSampleUnbufferedImpl(
      std::span<ValueTypeInBuffer> sample) -> bool;

  // Internal implementation of the private ReadSingleSample() which utilizes a
  // buffer to optimize the disk and memory access at the cost of extra stack
  // memory.
  // Expects that the sample is sized accordingly to the requested number of
  // samples and clamped to the actual spec number of samples. Does not perform
  // skip of unused channels.
  template <class ValueTypeInFile, class ValueTypeInBuffer>
  [[nodiscard]] auto ReadSingleSampleBufferedImpl(
      std::span<ValueTypeInBuffer> sample) -> bool;

  // Internal implementation of the public ReadAllSamples().
  //
  // Uses a value type used for storage as a template argument.
  template <class ValueTypeInFile,
            class ValueTypeInBuffer,
            size_t MaxChannels,
            class F,
            class... Args>
  [[nodiscard]] auto ReadAllSamples(F&& callback, Args&&... args) -> bool;

  // Internal implementation of the private ReadAllSamples() which reads
  // samples one by one directly from the disk. It is not very performant but
  // uses the least amount of stack size.
  template <class ValueTypeInFile,
            class ValueTypeInBuffer,
            size_t MaxChannels,
            class F,
            class... Args>
  [[nodiscard]] auto ReadAllSamplesUnbuffered(F&& callback, Args&&... args)
      -> bool;

  // Internal implementation of the private ReadAllSamples() which utilizes a
  // buffer to optimize the disk and memory access at the cost of extra stack
  // memory.
  template <class ValueTypeInFile,
            class ValueTypeInBuffer,
            size_t MaxChannels,
            class F,
            class... Args>
  [[nodiscard]] auto ReadAllSamplesBuffered(F&& callback, Args&&... args)
      -> bool;

  FileReader* file_reader_{nullptr};

  bool is_open_attempted_{false};  // True if Open() has been called.
  bool is_open_{false};            // True if the Open() returned true.

  // Endian in which multibyte values are stored in the file.
  std::endian file_endian_;

  FormatSpec format_spec_;

  // Total size of the DATA chunk is bytes and number of bytes read from it.
  // Used to detect when reading is to stop.
  uint64_t data_chunk_size_in_bytes_ = 0;
  uint64_t num_read_bytes_ = 0;
};

#if defined(_MSC_VER)
#  pragma warning(push)
// 4702 - Unreachable code.
//        MSVC might consider some fall-back code paths consider unreachable.
//        For example, assert() and default return value as a recovery.
//        Another example, is having unbuffered code after `if constexpr ()`
//        which evaluates to true for the buffered reading.
#  pragma warning(disable : 4702)
#endif

////////////////////////////////////////////////////////////////////////////////
// Public API implementation.

template <class FileReader>
auto Reader<FileReader>::Open(FileReader& file_reader) -> bool {
  assert(!is_open_attempted_);

  is_open_attempted_ = true;

  file_reader_ = &file_reader;
  is_open_ = ReadHeader();

  return is_open_;
}

template <class FileReader>
inline auto Reader<FileReader>::GetFormatSpec() const -> const FormatSpec& {
  assert(is_open_);

  return format_spec_;
}

template <class FileReader>
inline auto Reader<FileReader>::GetDurationInSeconds() const -> float {
  const uint32_t byte_depth = format_spec_.bit_depth / 8;

  const float num_samples_in_one_channel =
      data_chunk_size_in_bytes_ / format_spec_.num_channels / byte_depth;

  return num_samples_in_one_channel / format_spec_.sample_rate;
}

template <class FileReader>
template <class ValueType>
auto Reader<FileReader>::ReadSingleSample(const std::span<ValueType> sample)
    -> bool {
  switch (format_spec_.bit_depth) {
    case 16: return ReadSingleSample<int16_t, ValueType>(sample);
  }
  return false;
}

template <class FileReader>
template <class ValueType, size_t MaxChannels, class F, class... Args>
auto Reader<FileReader>::ReadAllSamples(F&& callback, Args&&... args) -> bool {
  switch (format_spec_.bit_depth) {
    case 16:
      return ReadAllSamples<int16_t, ValueType, MaxChannels>(
          std::forward<F>(callback), std::forward<Args>(args)...);
  }

  return false;
}

template <class FileReader>
template <class ValueType,
          size_t MaxChannels,
          class FileReaderType,
          class F,
          class... Args>
auto Reader<FileReader>::ReadAllSamples(FileReaderType&& file_reader,
                                        F&& callback,
                                        Args&&... args) -> bool {
  Reader<FileReader> reader;

  if (!reader.Open(file_reader)) {
    return false;
  }

  return reader.template ReadAllSamples<ValueType, MaxChannels>(
      std::forward<F>(callback), std::forward<Args>(args)...);
}

////////////////////////////////////////////////////////////////////////////////
// Private implementation.

namespace internal {

// Data structures used by a WAVE file are summarized in:
//
//   WAVE PCM soundfile format
//   http://soundfile.sapp.org/doc/WaveFormat/
//
// Some deviations are done to make chunk ID and size a more reusable entity
// other than operating on Chunk and Subchunk notation.
//
// In our terms the WAVE PCM file consists of chunks. Every chunk consists of
// its header and data. The required chunks are RIFF, FMT, and DATA.
//
// The RIFF chunk defines the top-level file information: the format used in the
// file. The RIFF chunk is identified as RIFF in files stored in the little
// endian and as RIFX for files stored in the big endian.
//
// The size stored in the RIFF chunk includes the RIFF data AND the rest of the
// file. In other words, the RIFF chunk acts as a container for the entire audio
// file.
//
// The FMT chunk contains channels and sampling specification (number of
// channels, sample rate, etc.).
//
// The DATA chunk contains samples.
//
// The file might have more chunks than the listed above. Although the ones from
// above are the required minimum for a proper audio file.

// Convert 4 character string representation of an ID to an unsigned 32-bit
// integer value in a way that it can be compared to a value directly read from
// file without runtime endian conversion.
constexpr auto IDStringToUInt32(const char a,
                                const char b,
                                const char c,
                                const char d) -> uint32_t {
  if constexpr (std::endian::native == std::endian::little) {
    return (uint8_t(a) << 0) | (uint8_t(b) << 8) | (uint8_t(c) << 16) |
           (uint8_t(d) << 24);
  }

  if constexpr (std::endian::native == std::endian::big) {
    return (uint8_t(a) << 24) | (uint8_t(b) << 16) | (uint8_t(c) << 8) |
           (uint8_t(d) << 0);
  }
}

// Mnemonics for known and supported chunk identifiers.
//
// Derived from the ASCII string representation stored as 32-bit integer.
//
// The value of ID matches the value when it is read from a binary file as
// uint32 without endian conversion.
enum class ChunkID : uint32_t {
  // RIFF corresponds to a WAVE files stored in little endian.
  kRIFF = IDStringToUInt32('R', 'I', 'F', 'F'),

  // RIFX is RIFF header but for a file using big endian.
  kRIFX = IDStringToUInt32('R', 'I', 'F', 'X'),

  // RF64 sisa header for 64-bit files.
  kRF64 = IDStringToUInt32('R', 'F', '6', '4'),

  // Data size 64.
  kDS64 = IDStringToUInt32('d', 's', '6', '4'),

  kFMT = IDStringToUInt32('f', 'm', 't', ' '),
  kDATA = IDStringToUInt32('d', 'a', 't', 'a'),
};

// Mnemonics for known and supported WAVE formats.
//
// Derived from the ASCII representation in the file stored as a hexadecimal
enum class Format : uint32_t {
  kWAVE = IDStringToUInt32('W', 'A', 'V', 'E'),
};

// Audio format stored in the FMT chunk.
enum class AudioFormat : uint16_t {
  kPCM = 1,
};

// Header of a chunk.
// Identifies what the coming chunk is and what its size is.
struct ChunkHeader {
  ChunkID id;     // Identifier of this chunk.
  uint32_t size;  // Size of the chunk in bytes.
};
static_assert(sizeof(ChunkHeader) == 8);

// Data of a RIFF chunk (`ChunkID::RIFF` for files stored in little endian and
// `ChunkID::RIFX` for files stored in big endian, `ChunkID::RF64` for 64-bit
// files).
struct RIFFData {
  Format format;  // Format of the file.
};
static_assert(sizeof(RIFFData) == 4);

// ds64 chunk.
// It  has to be the first chunk after the `RF64 chunk`.
struct DS64 {
  uint32_t riff_size_low;      // Low 4 byte size of RF64 block.
  uint32_t riff_size_high;     // High 4 byte size of RF64 block.
  uint32_t data_size_low;      // Low 4 byte size of data chunk.
  uint32_t data_size_high;     // High 4 byte size of data chunk.
  uint32_t sample_count_low;   // Low 4 byte sample count of fact chunk.
  uint32_t sample_count_high;  // High 4 byte sample count of fact chunk.
  uint32_t table_length;       // Number of valid entries in array “table”.

  // The table relies on the following type:
  //   struct ChunkSize64 {
  //     char chunkId[4];  // chunk ID (i.e. “big1” – this chunk is a big one)
  //     unsigned int32 chunkSizeLow;   // low 4 byte chunk size
  //     unsigned int32 chunkSizeHigh;  // high 4 byte chunk size
  //   };
  //
  // ChunkSize64 table[];
};
static_assert(sizeof(DS64) == 28);

struct FormatData {
  // Format in which audio data is stored.
  // Mnemonic representation is in `AudioFormat` enumerator.
  AudioFormat audio_format;

  // Number of channels.
  // Rather self-explanatory: 1 is mono, 2 is stereo, 5 is surround.
  uint16_t num_channels;

  // Sample rate in samples per second.
  uint32_t sample_rate;

  // sample_rate * num_channels * bit_depth/8
  uint32_t byte_rate;
  // num_channels * bit_depth/8
  uint16_t block_align;

  // Bits per sample of a single channel: 16 means samples are stored as 16-bit
  // integers.
  uint16_t bit_depth;
};
static_assert(sizeof(FormatData) == 16);

template <class T>
inline auto Byteswap(T value) -> T;

// Swap endianess of 16 bit unsigned int value.
template <>
[[nodiscard]] inline auto Byteswap(const uint16_t value) -> uint16_t {
#if defined(__GNUC__)
  return __builtin_bswap16(value);
#elif defined(_MSC_VER)
  static_assert(sizeof(unsigned short) == 2, "Size of short should be 2");
  return _byteswap_ushort(value);
#else
  return (value >> 8) | (value << 8);
#endif
}

// Swap endianess of 32 bit unsigned int value.
template <>
[[nodiscard]] inline auto Byteswap(const uint32_t value) -> uint32_t {
#if defined(__GNUC__)
  return __builtin_bswap32(value);
#elif defined(_MSC_VER)
  static_assert(sizeof(unsigned long) == 4, "Size of long should be 4");
  return _byteswap_ulong(value);
#else
  return ((value >> 24)) | ((value << 8) & 0x00ff0000) |
         ((value >> 8) & 0x0000ff00) | ((value << 24));
#endif
}

template <class T>
[[nodiscard]] inline auto Byteswap(const T value) -> T {
  static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4);

  if constexpr (sizeof(T) == 1) {
    return value;
  }

  if constexpr (sizeof(T) == 2) {
    uint16_t result = Byteswap(*reinterpret_cast<const uint16_t*>(&value));
    return *reinterpret_cast<T*>(&result);
  }

  if constexpr (sizeof(T) == 4) {
    uint32_t result = Byteswap(*reinterpret_cast<const uint32_t*>(&value));
    return *reinterpret_cast<T*>(&result);
  }

  assert(!"Unhandled type for Byteswap");

  return {};
}

// Get endian in which file is stored based on the RIFF header.
[[nodiscard]] inline auto EndianFromRIFFHeader(const ChunkHeader& riff_header)
    -> std::endian {
  if (riff_header.id == ChunkID::kRIFF || riff_header.id == ChunkID::kRF64) {
    return std::endian::little;
  }

  assert(riff_header.id == ChunkID::kRIFX);
  return std::endian::big;
}

// Check whether the format is supported by the reader.
[[nodiscard]] inline auto IsSupportedFormat(const FormatData& format_data)
    -> bool {
  // Only uncompressed PCM data is supported and uint16 samples are supported.

  if (format_data.audio_format != AudioFormat::kPCM) {
    return false;
  }

  if (format_data.bit_depth != 16) {
    return false;
  }

  return true;
}

[[nodiscard]] inline auto FormatDataToFormatSpec(const FormatData& format_data)
    -> FormatSpec {
  FormatSpec format_spec;

  assert(format_data.sample_rate < std::numeric_limits<int>::max());

  format_spec.num_channels = format_data.num_channels;
  format_spec.sample_rate = int(format_data.sample_rate);
  format_spec.bit_depth = format_data.bit_depth;

  return format_spec;
}

}  // namespace internal

template <class FileReader>
[[nodiscard]] inline auto Reader<FileReader>::ReadHeader() -> bool {
  // Read header performing initial configuration.

  internal::ChunkHeader riff_header;
  if (!ReadRIFFHeader(riff_header)) {
    return false;
  }

  internal::RIFFData riff_data;
  if (!ReadRIFFData(riff_data)) {
    return false;
  }
  if (riff_data.format != internal::Format::kWAVE) {
    return false;
  }

  // Read 64-bit information of the file.
  bool has_64bit_info = false;
  if (riff_header.id == internal::ChunkID::kRF64) {
    internal::DS64 ds64;
    if (!ReadDS64(ds64)) {
      return false;
    }
    data_chunk_size_in_bytes_ =
        uint64_t(ds64.data_size_high << 4) | ds64.data_size_low;
    has_64bit_info = true;
  }

  // Read format data and check that it is supported.

  internal::FormatData format_data;
  if (!SeekAndReadFormatData(format_data)) {
    return false;
  }

  if (!internal::IsSupportedFormat(format_data)) {
    return false;
  }

  // Position stream right at the beginning of the data.
  internal::ChunkHeader data_header;
  if (!SeekChunkID(internal::ChunkID::kDATA, data_header)) {
    return false;
  }

  // Set up the format.
  format_spec_ = FormatDataToFormatSpec(format_data);

  if (data_header.size != -1 || !has_64bit_info) {
    data_chunk_size_in_bytes_ = data_header.size;
  }

  return true;
}

template <class FileReader>
template <class T>
inline void Reader<FileReader>::FileToNativeEndian(T& value) {
  if (file_endian_ != std::endian::native) {
    value = internal::Byteswap(value);
  }
}

template <class FileReader>
template <class T>
[[nodiscard]] inline auto Reader<FileReader>::ReadObjectInMemory(T& object)
    -> bool {
  void* object_ptr = static_cast<void*>(&object);
  constexpr size_t kObjectSize = sizeof(T);

  return file_reader_->Read(object_ptr, kObjectSize) == kObjectSize;
}

template <class FileReader>
[[nodiscard]] inline auto Reader<FileReader>::ReadChunkHeader(
    internal::ChunkHeader& header) -> bool {
  if (!ReadObjectInMemory(header)) {
    return false;
  }

  FileToNativeEndian(header.size);

  return true;
}

template <class FileReader>
[[nodiscard]] inline auto Reader<FileReader>::ReadRIFFHeader(
    internal::ChunkHeader& riff_header) -> bool {
  if (!ReadObjectInMemory(riff_header)) {
    return false;
  }

  // Make sure this is an expected header.
  if (riff_header.id != internal::ChunkID::kRIFF &&
      riff_header.id != internal::ChunkID::kRIFX &&
      riff_header.id != internal::ChunkID::kRF64) {
    return false;
  }

  file_endian_ = EndianFromRIFFHeader(riff_header);

  FileToNativeEndian(riff_header.size);

  return true;
}

template <class FileReader>
[[nodiscard]] inline auto Reader<FileReader>::ReadRIFFData(
    internal::RIFFData& riff_data) -> bool {
  if (!ReadObjectInMemory(riff_data)) {
    return false;
  }

  FileToNativeEndian(riff_data.format);

  return true;
}

template <class FileReader>
[[nodiscard]] inline auto Reader<FileReader>::FileSkipNumBytes(
    const uint64_t num_bytes) -> bool {
  uint64_t num_remaining_bytes_to_skip = num_bytes;

  // Read in chunks of 4 bytes to minimize the number of reads.
  while (num_remaining_bytes_to_skip >= 4) {
    uint32_t unused;
    if (file_reader_->Read(static_cast<void*>(&unused), 4) != 4) {
      return false;
    }
    num_remaining_bytes_to_skip -= 4;
  }

  // Read remaining bytes.
  for (uint64_t i = 0; i < num_remaining_bytes_to_skip; ++i) {
    uint8_t unused;
    if (file_reader_->Read(static_cast<void*>(&unused), 1) != 1) {
      return false;
    }
  }

  return true;
}

template <class FileReader>
[[nodiscard]] inline auto Reader<FileReader>::SeekChunkID(
    const internal::ChunkID id, internal::ChunkHeader& header) -> bool {
  internal::ChunkHeader read_header;
  while (ReadChunkHeader(read_header)) {
    if (read_header.id == id) {
      header = read_header;
      return true;
    }

    if (!FileSkipNumBytes(read_header.size)) {
      return false;
    }
  }
  return false;
}

template <class FileReader>
[[nodiscard]] inline auto Reader<FileReader>::ReadDS64(internal::DS64& ds64)
    -> bool {
  // Read the header and ensure it is the expected one.
  internal::ChunkHeader ds64_header;
  if (!ReadObjectInMemory(ds64_header)) {
    return false;
  }
  if (ds64_header.id != internal::ChunkID::kDS64) {
    return false;
  }

  // Check the expected size of the chunk.
  // TODO(sergey): Support ds64 chunk with tables.
  if (ds64_header.size != sizeof(internal::DS64)) {
    return false;
  }

  if (!ReadObjectInMemory(ds64)) {
    return false;
  }

  FileToNativeEndian(ds64.riff_size_low);
  FileToNativeEndian(ds64.riff_size_high);
  FileToNativeEndian(ds64.data_size_low);
  FileToNativeEndian(ds64.data_size_high);
  FileToNativeEndian(ds64.sample_count_low);
  FileToNativeEndian(ds64.sample_count_high);
  FileToNativeEndian(ds64.table_length);

  // TODO(sergey): Read the table.

  return true;
}

template <class FileReader>
[[nodiscard]] inline auto Reader<FileReader>::SeekAndReadFormatData(
    internal::FormatData& format_data) -> bool {
  internal::ChunkHeader format_header;
  if (!SeekChunkID(internal::ChunkID::kFMT, format_header)) {
    // No format chunk found in the file.
    return false;
  }

  if (format_header.size != sizeof(internal::FormatData)) {
    return false;
  }

  if (!ReadObjectInMemory(format_data)) {
    return false;
  }

  FileToNativeEndian(format_data.audio_format);
  FileToNativeEndian(format_data.num_channels);
  FileToNativeEndian(format_data.sample_rate);
  FileToNativeEndian(format_data.byte_rate);
  FileToNativeEndian(format_data.block_align);
  FileToNativeEndian(format_data.bit_depth);

  return true;
}

namespace internal {

template <class FromType, class ToType>
auto ConvertValue(FromType sample) -> ToType;

template <>
inline auto ConvertValue(const int16_t sample) -> float {
  // There does not seem to be the standard and different implementations are
  // using different quantization rules.
  //
  // Follow the following
  //
  //   How to handle asymmetry of WAV data?
  //   https://gist.github.com/endolith/e8597a58bcd11a6462f33fa8eb75c43d
  //
  // which states that AES17 defines full-scale in a way that the most negative
  // value of a fixed integer type is unused.
  return float(sample) / 32767.0f;
}

template <>
inline auto ConvertValue(const int16_t sample) -> int16_t {
  return sample;
}

}  // namespace internal

template <class FileReader>
template <class ValueTypeInFile, class ValueTypeInBuffer>
[[nodiscard]] auto Reader<FileReader>::ReadSingleSample(
    const std::span<ValueTypeInBuffer> sample) -> bool {
  static_assert(TL_AUDIO_WAV_READER_SAMPLE_FRAME_BUFFER_SIZE >= 0);

  assert(is_open_);

  // Calculate minimum of the actual number of channels in the file and the
  // number of channels which is requested to be read.
  //
  // Upcast the actual number of channels to size_t so that comparison happens
  // within the same type, but keep result as uint32_t since this is what the
  // file format is operating in.
  const uint32_t num_channels_to_read =
      std::min(sample.size(), size_t(format_spec_.num_channels));

  std::span<ValueTypeInBuffer> sample_span =
      sample.subspan(0, num_channels_to_read);

  if constexpr (TL_AUDIO_WAV_READER_SAMPLE_FRAME_BUFFER_SIZE > 1) {
    if (!ReadSingleSampleBufferedImpl<ValueTypeInFile, ValueTypeInBuffer>(
            sample_span)) {
      return false;
    }
  } else {
    if (!ReadSingleSampleUnbufferedImpl<ValueTypeInFile, ValueTypeInBuffer>(
            sample_span)) {
      return false;
    }
  }

  if (format_spec_.num_channels == num_channels_to_read) {
    return true;
  }

  // If needed skip the remaining values, getting ready to read the next sample.
  const uint64_t num_bytes_to_skip =
      (format_spec_.num_channels - num_channels_to_read) *
      sizeof(ValueTypeInFile);
  return FileSkipNumBytes(num_bytes_to_skip);
}

template <class FileReader>
template <class ValueTypeInFile, class ValueTypeInBuffer>
[[nodiscard]] auto Reader<FileReader>::ReadSingleSampleUnbufferedImpl(
    const std::span<ValueTypeInBuffer> sample) -> bool {
  const size_t num_channels_to_read = sample.size();
  for (size_t i = 0; i < num_channels_to_read; ++i) {
    ValueTypeInFile sample_from_file;
    if (!ReadObjectInMemory(sample_from_file)) {
      return false;
    }
    FileToNativeEndian(sample_from_file);

    sample[i] = internal::ConvertValue<ValueTypeInFile, ValueTypeInBuffer>(
        sample_from_file);

    num_read_bytes_ += sizeof(ValueTypeInFile);
  }

  return true;
}

template <class FileReader>
template <class ValueTypeInFile, class ValueTypeInBuffer>
[[nodiscard]] auto Reader<FileReader>::ReadSingleSampleBufferedImpl(
    const std::span<ValueTypeInBuffer> sample) -> bool {
  static_assert(TL_AUDIO_WAV_READER_SAMPLE_FRAME_BUFFER_SIZE >= 0);

  // NOTE: It is tempting to wrap buffer reading and endian to a utility
  // function, but it seems to lead to a poor performance. Presumable because of
  // extra math needed to maintain the num_read_bytes_ with the implementation
  // of the reading utility matching behavior with ReadObjectInMemory.

  assert(is_open_);

  // This is more of provisionary check, to avoid possible warnings about zero
  // sized buffer.
  if constexpr (TL_AUDIO_WAV_READER_SAMPLE_FRAME_BUFFER_SIZE > 1) {
    const size_t num_channels_to_read = sample.size();

    // Buffer for reading from the file.
    std::array<ValueTypeInFile, TL_AUDIO_WAV_READER_SAMPLE_FRAME_BUFFER_SIZE>
        buffer;

    if (num_channels_to_read <= TL_AUDIO_WAV_READER_SAMPLE_FRAME_BUFFER_SIZE) {
      // An optimized version with less branching for cases when the
      // frame-sample fits into the buffer.

      const size_t num_bytes_to_read =
          num_channels_to_read * sizeof(ValueTypeInFile);
      if (file_reader_->Read(buffer.data(), num_bytes_to_read) !=
          num_bytes_to_read) {
        return false;
      }
      // Endian conversion.
      // To single endian check before entering the loop to avoid a no-op loop
      // if the WAV endianess matches the current system endianess.
      if (file_endian_ != std::endian::native) {
        for (size_t i = 0; i < num_channels_to_read; ++i) {
          FileToNativeEndian(buffer[i]);
        }
      }
      // Convert type from how it's stored in the file to the return sample
      // type.
      for (size_t i = 0; i < num_channels_to_read; ++i) {
        sample[i] = internal::ConvertValue<ValueTypeInFile, ValueTypeInBuffer>(
            buffer[i]);
      }
      num_read_bytes_ += num_bytes_to_read;
      return true;
    }

    // A non-optimized version which handles an arbitrary number of channels.

    size_t num_channels_read = 0;
    while (num_channels_read != num_channels_to_read) {
      const size_t n =
          std::min(size_t(TL_AUDIO_WAV_READER_SAMPLE_FRAME_BUFFER_SIZE),
                   num_channels_to_read - num_channels_read);
      const size_t num_bytes_to_read = n * sizeof(ValueTypeInFile);
      if (file_reader_->Read(buffer.data(), num_bytes_to_read) !=
          num_bytes_to_read) {
        return false;
      }
      // Endian conversion.
      // To single endian check before entering the loop to avoid a no-op loop
      // if the WAV endianess matches the current system endianess.
      if (file_endian_ != std::endian::native) {
        for (size_t i = 0; i < n; ++i) {
          FileToNativeEndian(buffer[i]);
        }
      }
      // Convert type from how it's stored in the file to the return sample
      // type.
      for (size_t i = 0; i < n; ++i) {
        sample[num_channels_read++] =
            internal::ConvertValue<ValueTypeInFile, ValueTypeInBuffer>(
                buffer[i]);
      }
      num_read_bytes_ += num_bytes_to_read;
    }
    return true;
  }

  return false;
}

template <class FileReader>
template <class ValueTypeInFile,
          class ValueTypeInBuffer,
          size_t MaxChannels,
          class F,
          class... Args>
[[nodiscard]] auto Reader<FileReader>::ReadAllSamples(F&& callback,
                                                      Args&&... args) -> bool {
  static_assert(TL_AUDIO_WAV_READER_BUFFER_SIZE >= 0);

  if constexpr (TL_AUDIO_WAV_READER_BUFFER_SIZE > 1) {
    return ReadAllSamplesBuffered<ValueTypeInFile,
                                  ValueTypeInBuffer,
                                  MaxChannels>(std::forward<F>(callback),
                                               std::forward<Args>(args)...);
  }

  return ReadAllSamplesUnbuffered<ValueTypeInFile,
                                  ValueTypeInBuffer,
                                  MaxChannels>(std::forward<F>(callback),
                                               std::forward<Args>(args)...);
}

template <class FileReader>
template <class ValueTypeInFile,
          class ValueTypeInBuffer,
          size_t MaxChannels,
          class F,
          class... Args>
[[nodiscard]] auto Reader<FileReader>::ReadAllSamplesUnbuffered(F&& callback,
                                                                Args&&... args)
    -> bool {
  // TODO(sergey): Why is it is_open_attempted_ and not is_open_?
  assert(is_open_attempted_);

  // Buffer of a single sample of an audio frame with the maximum configured
  // number of channels. The rest of the channels from the sample frame are
  // ignored.
  using SampleBuffer = std::array<ValueTypeInBuffer, MaxChannels>;
  SampleBuffer samples_buffer;

  // Create a view of the buffer which matches the number of usable channels:
  // at maximum of the number of channels in the file, but not exceeding the
  // sample buffer size.
  const size_t num_usable_channels =
      std::min(samples_buffer.size(), size_t(format_spec_.num_channels));
  std::span<ValueTypeInBuffer> samples{samples_buffer.data(),
                                       num_usable_channels};

  while (num_read_bytes_ < data_chunk_size_in_bytes_) {
    if (!ReadSingleSample<ValueTypeInFile, ValueTypeInBuffer>(samples)) {
      return false;
    }

    std::invoke(
        std::forward<F>(callback), std::forward<Args>(args)..., samples);
  }

  return true;
}

template <class FileReader>
template <class ValueTypeInFile,
          class ValueTypeInBuffer,
          size_t MaxChannels,
          class F,
          class... Args>
[[nodiscard]] auto Reader<FileReader>::ReadAllSamplesBuffered(F&& callback,
                                                              Args&&... args)
    -> bool {
  static_assert(TL_AUDIO_WAV_READER_BUFFER_SIZE >= 0);

  // TODO(sergey): Why is it is_open_attempted_ and not is_open_?
  assert(is_open_attempted_);

  if constexpr (TL_AUDIO_WAV_READER_BUFFER_SIZE > 1) {
    // Buffer of a single sample of an audio frame with the maximum configured
    // number of channels. The rest of the channels from the sample frame are
    // ignored.
    using SampleBuffer = std::array<ValueTypeInBuffer, MaxChannels>;
    SampleBuffer samples_buffer;

    // Create a view of the buffer which matches the number of usable channels:
    // at maximum of the number of channels in the file, but not exceeding the
    // sample buffer size.
    const size_t num_usable_channels =
        std::min(samples_buffer.size(), size_t(format_spec_.num_channels));
    std::span<ValueTypeInBuffer> samples{samples_buffer.data(),
                                         num_usable_channels};

    // The number of channels to be ignored at the end of the frame.
    // This is a non-zero value if the file contains more channels than
    // requested.
    const size_t num_skip_channels =
        format_spec_.num_channels - num_usable_channels;

    // The number of frames of full channels that can be stored in the buffer.
    const size_t num_frames_in_buffer =
        TL_AUDIO_WAV_READER_BUFFER_SIZE / format_spec_.num_channels;

    // If the buffer can not be efficiently utilized fallback to a
    // sample-by-sample reader.
    if (num_frames_in_buffer <= 1) {
      return ReadAllSamplesUnbuffered<ValueTypeInFile,
                                      ValueTypeInBuffer,
                                      MaxChannels>(std::forward<F>(callback),
                                                   std::forward<Args>(args)...);
    }

    // Buffer for reading from the file.
    std::array<ValueTypeInFile, TL_AUDIO_WAV_READER_BUFFER_SIZE> buffer;

    // The number of bytes in a single frame.
    const size_t num_bytes_in_frame =
        format_spec_.num_channels * sizeof(ValueTypeInFile);

    size_t num_remained_frames = (data_chunk_size_in_bytes_ - num_read_bytes_) /
                                 sizeof(ValueTypeInFile) /
                                 format_spec_.num_channels;
    while (num_remained_frames > 0) {
      // Clamp the number of frames by the apparent remaining number of frames.
      const size_t num_frames_to_read =
          std::min(num_remained_frames, num_frames_in_buffer);
      const size_t num_bytes_to_read = num_frames_to_read * num_bytes_in_frame;

      if (file_reader_->Read(buffer.data(), num_bytes_to_read) !=
          num_bytes_to_read) {
        return false;
      }

      // The total number of samples in the read frames.
      const size_t num_samples_read = num_frames_to_read * num_usable_channels;

      // Endian conversion.
      // To single endian check before entering the loop to avoid a no-op loop
      // if the WAV endianess matches the current system endianess.
      //
      // TODO(sergey): Make it efficient if the number of ignored channels is
      // high.
      if (file_endian_ != std::endian::native) {
        for (size_t i = 0; i < num_samples_read; ++i) {
          FileToNativeEndian(buffer[i]);
        }
      }

      for (size_t frame = 0, frame_sample = 0; frame < num_frames_to_read;
           ++frame) {
        // Convert sample which will be passed to the callback from file data
        // type to the type which the callback expects.
        for (size_t i = 0; i < num_usable_channels; ++i, ++frame_sample) {
          samples[i] =
              internal::ConvertValue<ValueTypeInFile, ValueTypeInBuffer>(
                  buffer[frame_sample]);
        }
        frame_sample += num_skip_channels;

        std::invoke(
            std::forward<F>(callback), std::forward<Args>(args)..., samples);
      }

      num_remained_frames -= num_frames_to_read;
      num_read_bytes_ += num_bytes_to_read;
    }

    // It is expected that the file consists of an integer number of frames.
    // If there are bytes left it means there are samples that do not form a
    // whole frame.
    if (num_read_bytes_ != data_chunk_size_in_bytes_) {
      return false;
    }

    return true;
  }

  return false;
}

#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

}  // namespace TL_AUDIO_WAV_READER_VERSION_NAMESPACE
}  // namespace TL_AUDIO_WAV_READER_NAMESPACE

#undef TL_AUDIO_WAV_READER_VERSION_MAJOR
#undef TL_AUDIO_WAV_READER_VERSION_MINOR
#undef TL_AUDIO_WAV_READER_VERSION_REVISION

#undef TL_AUDIO_WAV_READER_SAMPLE_FRAME_BUFFER_SIZE

#undef TL_AUDIO_WAV_READER_NAMESPACE

#undef TL_AUDIO_WAV_READER_VERSION_NAMESPACE_CONCAT_HELPER
#undef TL_AUDIO_WAV_READER_VERSION_NAMESPACE_CONCAT
#undef TL_AUDIO_WAV_READER_VERSION_NAMESPACE
