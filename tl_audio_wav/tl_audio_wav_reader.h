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
// - Only uncompressed PCM 16 bit signed integer data type is supported.
//
// - Streaming is not optimized: it can take substantial time to read int16 file
//   as float samples.
//
//
// Version history
// ===============
//
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

// Semantic version of the tl_audio_wav_reader library.
#define TL_AUDIO_WAV_READER_VERSION_MAJOR 0
#define TL_AUDIO_WAV_READER_VERSION_MINOR 0
#define TL_AUDIO_WAV_READER_VERSION_REVISION 1

// Namespace of the module.
// The outer name spaces which surrounds the ABI-version namespace.
#ifndef TL_AUDIO_WAV_READER_NAMESPACE
#  define TL_AUDIO_WAV_READER_NAMESPACE tiny_lib::audio_wav_reader
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
  // For example: 16 means values are stored as 16 bit signed integers.
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
  // from the file. Namely that the samples are stored in a supported format.
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
  inline auto ReadHeader() -> bool;

  // Convert value from how they were read from file to the native endianess.
  template <class T>
  inline void FileToNativeEndian(T& value);

  // Read object from file stream directly into memory, without endian
  // conversion or any other semantic checks.
  //
  // Returns true if the object is fully read, false otherwise.
  template <class T>
  inline auto ReadObjectInMemory(T& object) -> bool;

  // Rad chunk header from the file
  inline auto ReadChunkHeader(internal::ChunkHeader& header) -> bool;

  // Read the RIFF/RIFX header.
  // Upon success will initialize the file endian.
  inline auto ReadRIFFHeader(internal::ChunkHeader& riff_header) -> bool;

  // Read the RIFF/RIFX data.
  inline auto ReadRIFFData(internal::RIFFData& riff_data) -> bool;

  // Skip given number of bytes from the current location of file.
  // Returns true if all requested bytes has been skipped.
  inline auto FileSkipNumBytes(uint32_t num_bytes) -> bool;

  // Read the file seeking for a chunk header of given ID.
  //
  // If the chunk is found the reader is positioned right after it and true
  // is returned.
  inline auto SeekChunkID(internal::ChunkID id, internal::ChunkHeader& header)
      -> bool;

  // Seek for a format chunk and read its data.
  inline auto SeekAndReadFormatData(internal::FormatData& format_data) -> bool;

  // Internal implementation of the public ReadSingleSample().
  //
  // Uses a value type used for storage as a template argument.
  template <class ValueTypeInFile, class ValueTypeInBuffer>
  auto ReadSingleSample(std::span<ValueTypeInBuffer> sample) -> bool;

  // Internal implementation of the public ReadAllSamples().
  //
  // Uses a value type used for storage as a template argument.
  template <class ValueTypeInFile,
            class ValueTypeInBuffer,
            size_t MaxChannels,
            class F,
            class... Args>
  auto ReadAllSamples(F&& callback, Args&&... args) -> bool;

  FileReader* file_reader_{nullptr};

  bool is_open_attempted_{false};  // True if Open() has been called.
  bool is_open_{false};            // True if the Open() returned true.

  // Endian in which multi-byte values are stored in the file.
  std::endian file_endian_;

  FormatSpec format_spec_;

  // Total size of the DATA chunk is bytes and number of bytes read from it.
  // Used to detect when reading is to stop.
  uint32_t data_chunk_size_in_bytes_ = 0;
  uint32_t num_read_bytes_ = 0;
};

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

  const uint32_t num_samples_in_one_channel =
      data_chunk_size_in_bytes_ / format_spec_.num_channels / byte_depth;

  return float(num_samples_in_one_channel) / format_spec_.sample_rate;
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

  return reader.ReadAllSamples<ValueType, MaxChannels>(
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
// channels, sample rate, etc).
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
// Derived from the ASCII string representation stored as 32 bit integer.
//
// The value of ID matches the value when it is read from a binary file as
// uint32 without endian conversion.
enum class ChunkID : uint32_t {
  // RIFF corresponds to a WAVE files stored in little endian.
  kRIFF = IDStringToUInt32('R', 'I', 'F', 'F'),

  // RIFX is RIFF header but for a file using big endian.
  kRIFX = IDStringToUInt32('R', 'I', 'F', 'X'),

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
// `ChunkID::RIFX` for files stored in big endian).
struct RIFFData {
  Format format;  // Format of the file.
};
static_assert(sizeof(RIFFData) == 4);

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

  // Bits per sample of a single channel: 16 means samples are stored as 16 bit
  // integers.
  uint16_t bit_depth;
};
static_assert(sizeof(FormatData) == 16);

template <class T>
inline auto Byteswap(T value) -> T;

// Swap endianess of 16 bit unsigned int value.
template <>
inline auto Byteswap(const uint16_t value) -> uint16_t {
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
inline auto Byteswap(const uint32_t value) -> uint32_t {
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
inline auto Byteswap(const T value) -> T {
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
}

// Get endian in which file is stored based on the RIFF header.
inline auto EndianFromRIFFHeader(const ChunkHeader& riff_header)
    -> std::endian {
  if (riff_header.id == ChunkID::kRIFF) {
    return std::endian::little;
  }

  assert(riff_header.id == ChunkID::kRIFX);
  return std::endian::big;
}

// Check whether the format is supported by the reader.
inline auto IsSupportedFormat(const FormatData& format_data) -> bool {
  // Only uncompressed PCM data is supported and uint16 samples are supported.

  if (format_data.audio_format != AudioFormat::kPCM) {
    return false;
  }

  if (format_data.bit_depth != 16) {
    return false;
  }

  return true;
}

inline auto FormatDataToFormatSpec(const FormatData& format_data)
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
inline auto Reader<FileReader>::ReadHeader() -> bool {
  // Read header performing initial configuration.

  internal::ChunkHeader riff_header;
  if (!ReadRIFFHeader(riff_header)) {
    return false;
  }

  internal::RIFFData riff_data;
  if (!ReadRIFFData(riff_data)) {
    return false;
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

  data_chunk_size_in_bytes_ = data_header.size;

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
inline auto Reader<FileReader>::ReadObjectInMemory(T& object) -> bool {
  void* object_ptr = static_cast<void*>(&object);
  constexpr size_t kObjectSize = sizeof(T);

  return file_reader_->Read(object_ptr, kObjectSize) == kObjectSize;
}

template <class FileReader>
inline auto Reader<FileReader>::ReadChunkHeader(internal::ChunkHeader& header)
    -> bool {
  if (!ReadObjectInMemory(header)) {
    return false;
  }

  FileToNativeEndian(header.size);

  return true;
}

template <class FileReader>
inline auto Reader<FileReader>::ReadRIFFHeader(
    internal::ChunkHeader& riff_header) -> bool {
  if (!ReadObjectInMemory(riff_header)) {
    return false;
  }

  // Make sure this is an expected header.
  if (riff_header.id != internal::ChunkID::kRIFF &&
      riff_header.id != internal::ChunkID::kRIFX) {
    return false;
  }

  file_endian_ = EndianFromRIFFHeader(riff_header);

  FileToNativeEndian(riff_header.size);

  return true;
}

template <class FileReader>
inline auto Reader<FileReader>::ReadRIFFData(internal::RIFFData& riff_data)
    -> bool {
  return ReadObjectInMemory(riff_data);
}

template <class FileReader>
inline auto Reader<FileReader>::FileSkipNumBytes(const uint32_t num_bytes)
    -> bool {
  uint32_t num_remaining_bytes_to_skip = num_bytes;

  // Read in chunks of 4 bytes to minimize the number of reads.
  while (num_remaining_bytes_to_skip >= 4) {
    uint32_t unused;
    if (file_reader_->Read(static_cast<void*>(&unused), 4) != 4) {
      return false;
    }
    num_remaining_bytes_to_skip -= 4;
  }

  // Read remaining bytes.
  for (uint32_t i = 0; i < num_remaining_bytes_to_skip; ++i) {
    uint8_t unused;
    if (file_reader_->Read(static_cast<void*>(&unused), 1) != 1) {
      return false;
    }
  }

  return true;
}

template <class FileReader>
inline auto Reader<FileReader>::SeekChunkID(const internal::ChunkID id,
                                            internal::ChunkHeader& header)
    -> bool {
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
inline auto Reader<FileReader>::SeekAndReadFormatData(
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
auto ConvertValue(const int16_t sample) -> float {
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
auto ConvertValue(const int16_t sample) -> int16_t {
  return sample;
}

}  // namespace internal

template <class FileReader>
template <class ValueTypeInFile, class ValueTypeInBuffer>
auto Reader<FileReader>::ReadSingleSample(
    const std::span<ValueTypeInBuffer> sample) -> bool {
  assert(is_open_);

  // Calculate minimum of the actual number of channels in the file and the
  // number of channels which is requested to be read.
  //
  // Upcast the actual number of channels to size_t so that comparison happens
  // within the same type, but keep result as uint32_t since this is what the
  // file format is operating in.
  const uint32_t num_values_to_read =
      std::min(sample.size(), size_t(format_spec_.num_channels));

  // Read per-channel values of the sample and fill in the buffer.
  for (uint32_t i = 0; i < num_values_to_read; ++i) {
    ValueTypeInFile sample_from_file;
    if (!ReadObjectInMemory(sample_from_file)) {
      return false;
    }
    FileToNativeEndian(sample_from_file);

    sample[i] = internal::ConvertValue<ValueTypeInFile, ValueTypeInBuffer>(
        sample_from_file);

    num_read_bytes_ += sizeof(ValueTypeInFile);
  }

  // If needed skip the remaining values, getting ready to read the next sample.
  const uint32_t num_bytes_to_skip =
      (format_spec_.num_channels - num_values_to_read) *
      sizeof(ValueTypeInFile);
  return FileSkipNumBytes(num_bytes_to_skip);
}

template <class FileReader>
template <class ValueTypeInFile,
          class ValueTypeInBuffer,
          size_t MaxChannels,
          class F,
          class... Args>
auto Reader<FileReader>::ReadAllSamples(F&& callback, Args&&... args) -> bool {
  assert(is_open_attempted_);

  // Buffer of a static size allocated for a maximum supported number of
  // channels.
  std::array<ValueTypeInBuffer, MaxChannels> samples_buffer;

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

}  // namespace TL_AUDIO_WAV_READER_VERSION_NAMESPACE
}  // namespace TL_AUDIO_WAV_READER_NAMESPACE

#undef TL_AUDIO_WAV_READER_VERSION_MAJOR
#undef TL_AUDIO_WAV_READER_VERSION_MINOR
#undef TL_AUDIO_WAV_READER_VERSION_REVISION

#undef TL_AUDIO_WAV_READER_NAMESPACE

#undef TL_AUDIO_WAV_READER_VERSION_NAMESPACE_CONCAT_HELPER
#undef TL_AUDIO_WAV_READER_VERSION_NAMESPACE_CONCAT
#undef TL_AUDIO_WAV_READER_VERSION_NAMESPACE
