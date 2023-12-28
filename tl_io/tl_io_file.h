// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

// File read and write implementation. It supports the following:
//
//   - RAII style file descriptor management.
//   - Cross-platform support of access to non-ASCII file names.
//   - Cross-platform access to files which are bigger than 4 GiB.
//
// This file implementation can also be used as IO interface for other tiny lib
// libraries.
//
//
// Limitations
// ===========
//
//  - std::filesystem passed to the functions needs to be handled with extra
//    care: the new style of path creation with MSVC does not seem to properly
//    handle non-ASCII paths. Using now-deprecated using std::filesystem::u8path
//    to construct path from string works correctly.
//
//  - The API defines data as 64bit, but the underlying file access relies on
//    the global configuration. To get advantage of big files on 32bit POSIX
//    systems `_FILE_OFFSET_BITS 64` needs to be defined to get full support
//    of large files.
//
//
// Version history
// ===============
//
//   0.0.1-alpha    (28 Dec 2023)    First public release.

#pragma once

#include <fcntl.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <span>

// Semantic version of the tl_io_file library.
#define TL_IO_FILE_VERSION_MAJOR 0
#define TL_IO_FILE_VERSION_MINOR 0
#define TL_IO_FILE_VERSION_REVISION 1

// Namespace of the module.
// The outer name spaces which surrounds the ABI-version namespace.
#ifndef TL_IO_FILE_NAMESPACE
#  define TL_IO_FILE_NAMESPACE tiny_lib::io_file
#endif

// Helpers for TL_IO_FILE_VERSION_NAMESPACE.
//
// Typical extra indirection for such conversion to allow macro to be expanded
// before it is converted to string.
#define TL_IO_FILE_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)              \
  v_##id1##_##id2##_##id3
#define TL_IO_FILE_VERSION_NAMESPACE_CONCAT(id1, id2, id3)                     \
  TL_IO_FILE_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)

// Constructs identifier suitable for namespace denoting the current library
// version.
//
// For example: TL_IO_FILE_VERSION_NAMESPACE -> v_0_1_9
#define TL_IO_FILE_VERSION_NAMESPACE                                           \
  TL_IO_FILE_VERSION_NAMESPACE_CONCAT(TL_IO_FILE_VERSION_MAJOR,                \
                                      TL_IO_FILE_VERSION_MINOR,                \
                                      TL_IO_FILE_VERSION_REVISION)

#if defined(_MSC_VER)
#  define TL_IO_FILE_COMPILER_MSVC 1
#else
#  define TL_IO_FILE_COMPILER_MSVC 0
#endif

// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace TL_IO_FILE_NAMESPACE {
inline namespace TL_IO_FILE_VERSION_NAMESPACE {

////////////////////////////////////////////////////////////////////////////////
// Public API declaration.

class File {
 public:
  using PositionType = int64_t;
  using OffsetType = int64_t;
  using SizeType = size_t;

  // Bit flags defining access mode and disposition for file open.
  //
  // Typical mapping from fopen() mode:
  //
  //   - rb  : File::kRead
  //   - wb  : File::kWrite | File::kCreateAlways
  //   - ab  : File::kAppend | File::kOpenAlways
  //   - rb+ : File::kRead | File::kWrite
  //   - wb+ : File::kRead | File::kWrite | File::kOpenAlways
  //   - ab+ : File::kRead | File::kAppend | File::kOpenAlways
  enum Flags {
    // Open disposition.

    // Creates a new file, only if it does not already exist.
    kCreate = (1 << 0),

    // Create regular file if it doesn't exist, then open it.
    kOpenAlways = (1 << 1),

    // If the file exists, it will be truncated to 0 length.
    // If the file does not exist has the same behavior as kCreate.
    //
    // The way to think of the result: the file will be ensured to be created
    // and it will be 0 size.
    kCreateAlways = (1 << 2),

    // If the file exist, open the file and truncate it.
    // If the file does not exist return an error.
    kOpenTruncated = (1 << 3),

    kDispositionBits = (kCreate | kOpenAlways | kCreateAlways | kOpenTruncated),

    // Access mode.

    kRead = (1 << 4),
    kWrite = (1 << 5),
    kAppend = (1 << 6),

    kAccessBits = (kRead | kWrite | kAppend),
  };

  enum class Whence {
    // Seek will happen at the provided absolute position starting from the
    // beginning of the file.
    kBeginning,

    // Seek will happen at the relative positive measured to the current
    // position.
    kCurrent,

    // Seek will happen at the provided absolute position starting from the
    // end of the file.
    kEnd,
  };

  File() = default;

  inline File(File&& other) noexcept;
  inline auto operator=(File&& other) -> File&;

  // Disallow copy as copying file descriptor needs some special handling.
  // Might be implemented in the future.
  File(const File& other) = delete;
  auto operator=(const File& other) -> File& = delete;

  // Closes all open file descriptors.
  inline ~File();

  // Open file for access in the given mode.
  //
  // NOTE: When the filename is constructed from a string it is expected that
  // std::filesystem::u8path is used. Otherwise non-ASCII paths will not be
  // handled correctly.
  //
  // Returns true on success.
  inline auto Open(const std::filesystem::path& filename, int flags) -> bool;

  // Close the file if it is open.
  //
  // Returns true on success.
  // If the file is not open it is considered successfully closed.
  inline auto Close() -> bool;

  // Move the current file position to a position measured in bytes obtained
  // from the given offset measured relative to the `whence`.
  //
  // Returns true on success.
  inline auto Seek(OffsetType offset, Whence whence) -> bool;

  // Rewind the file to its beginning.
  inline auto Rewind() -> bool;

  // Returns the current position in the file in bytes from the beginning of the
  // file.
  // Upon failure returns -1.
  inline auto Tell() -> OffsetType;

  // Calculate size of the file in bytes.
  // Upon failure returns -1.
  inline auto Size() -> OffsetType;

  // Read given number of bytes starting form the current file position into the
  // given buffer.
  //
  // Returns the number of bytes actually read.
  //
  // If an error occurs, or the end-of-file is reached, the return value is a
  // short bytes count or a zero.
  //
  // This call does not distinguish the end-of-file from an error and the a
  // caller is to use IsEOF() and IsError() to determine which occurred.
  inline auto Read(void* ptr, SizeType num_bytes_to_read) -> SizeType;

  // Write given number of bytes form the buffer into the file starting from the
  // current position.
  //
  // Returns the number of bytes actually written.
  //
  // If an error occurs the return value is a short bytes count or zero.
  //
  // The return number of bytes is only lower than the requested number if an
  // error occurs.
  inline auto Write(const void* ptr, SizeType num_bytes_to_write) -> SizeType;

  // Returns true if the file has end-of-file indicator.
  //
  // Note that stream's internal position indicator may point to the end-of-file
  // for the next operation, but still, the end-of-file indicator may not be set
  // until an operation attempts to read at that point.
  inline auto IsEOF() -> bool;

  // Returns true if the file is in an error state.
  inline auto IsError() -> bool;

  // Read file as a text into the given destination.
  //
  // If the file is larger than the text.max_size() then false is returned.
  // The text is resized to the file size using text.resize().
  //
  // NOTE: When the filename is constructed from a string it is expected that
  // std::filesystem::u8path is used. Otherwise non-ASCII paths will not be
  // handled correctly.
  //
  // Returns true on success.
  template <class StringType>
  static auto ReadText(const std::filesystem::path& filename, StringType& text)
      -> bool;

  // Read file as a binary data into the given destination.
  //
  // If the file is larger than the buffer.max_size() then false is returned.
  // The buffer is resized to the file size using buffer.resize().
  //
  // NOTE: When the filename is constructed from a string it is expected that
  // std::filesystem::u8path is used. Otherwise non-ASCII paths will not be
  // handled correctly.
  //
  // Returns true on success.
  template <class BufferType>
  static auto ReadBytes(const std::filesystem::path& filename,
                        BufferType& buffer) -> bool;

  // Write text to into the file destination.
  //
  // NOTE: When the filename is constructed from a string it is expected that
  // std::filesystem::u8path is used. Otherwise non-ASCII paths will not be
  // handled correctly.
  //
  // Returns true on success.
  template <class StringType>
  static auto WriteText(const std::filesystem::path& filename,
                        const StringType& text) -> bool;

  // Write binary data to the file.
  //
  // NOTE: When the filename is constructed from a string it is expected that
  // std::filesystem::u8path is used. Otherwise non-ASCII paths will not be
  // handled correctly.
  //
  // Returns true on success.
  template <class BufferType>
  static auto WriteBytes(const std::filesystem::path& filename,
                         const BufferType& buffer) -> bool;

 private:
  FILE* file_stream_{nullptr};
};

////////////////////////////////////////////////////////////////////////////////
// Implementation.

namespace internal {

// Compile-time trick to get char* mode on POSIX and wchar_t* mode when using
// MSVC.

#if TL_IO_FILE_COMPILER_MSVC
using ModeCharT = wchar_t;
#else
using ModeCharT = char;
#endif

template <class T>
struct ChooseMode;

template <>
struct ChooseMode<char> {
  static constexpr auto Str(const char* narrow, const wchar_t* /*wide*/)
      -> const char* {
    return narrow;
  }
};

template <>
struct ChooseMode<wchar_t> {
  static constexpr auto Str(const char* /*narrow*/, const wchar_t* wide)
      -> const wchar_t* {
    return wide;
  }
};

// Convert bitmask of File::Flag to mode suitable for fopen().
inline auto OpenFlagsToMode(const int flags) -> const ModeCharT* {
  switch (flags) {
    case File::kRead: return ChooseMode<ModeCharT>::Str("rb", L"rb");
    case File::kWrite | File::kCreateAlways:
      return ChooseMode<ModeCharT>::Str("wb", L"wb");
    case File::kAppend | File::kOpenAlways:
      return ChooseMode<ModeCharT>::Str("ab", L"ab");
    case File::kRead | File::kWrite:
      return ChooseMode<ModeCharT>::Str("rb+", L"rb+");
    case File::kRead | File::kWrite | File::kOpenAlways:
      return ChooseMode<ModeCharT>::Str("wb+", L"wb+");
    case File::kRead | File::kAppend | File::kOpenAlways:
      return ChooseMode<ModeCharT>::Str("ab+", L"ab+");
  }

  assert(!"Unsupported flags combination.");
  return ChooseMode<ModeCharT>::Str("", L"");
}

// Convert strongly typed own mnemonic for Whence to a posix value of SEEK_SET,
// SEEK_CUR, or SEEK_END.
inline auto WhenceToPOSIX(const File::Whence whence) -> int {
  switch (whence) {
    case File::Whence::kBeginning: return SEEK_SET;
    case File::Whence::kCurrent: return SEEK_CUR;
    case File::Whence::kEnd: return SEEK_END;
  }

  assert(!"Unreachable code");

  return -1;
}

}  // namespace internal

File::File(File&& other) noexcept : file_stream_{other.file_stream_} {
  other.file_stream_ = nullptr;
}

auto File::operator=(File&& other) -> File& {
  if (this == &other) {
    return *this;
  }

  file_stream_ = other.file_stream_;

  other.file_stream_ = nullptr;

  return *this;
}

File::~File() { Close(); }

auto File::Open(const std::filesystem::path& filename, const int flags)
    -> bool {
  Close();

#if TL_IO_FILE_COMPILER_MSVC
  // Use deprecated std::filesystem::u8path as it seems to be the only way to
  // support non-ASCII file names.
  //
  // Using both u8string and string to create path and pass path.c_str() to any
  // function which expects wide string (_wsopen_s, CreateFileW, ...) does not
  // handle Unicode names correctly.
#  pragma warning(push)
#  pragma warning(disable : 4996)

  const wchar_t* mode = internal::OpenFlagsToMode(flags);

  const errno_t error = ::_wfopen_s(
      &file_stream_, std::filesystem::u8path(filename.string()).c_str(), mode);

#  pragma warning(pop)

  if (error != 0) {
    return false;
  }
#else
  const char* mode = internal::OpenFlagsToMode(flags);
  file_stream_ = ::fopen(filename.c_str(), mode);
#endif

  return file_stream_ != nullptr;
}

auto File::Close() -> bool {
  if (file_stream_ == nullptr) {
    return true;
  }

  if (::fclose(file_stream_) != 0) {
    return false;
  }

  file_stream_ = nullptr;

  return true;
}

// Semantically it is not const, as the position within the file changes.
// NOLINTNEXTLINE(readability-make-member-function-const)
auto File::Seek(const OffsetType offset, const Whence whence) -> bool {
  const int posix_whence = internal::WhenceToPOSIX(whence);
#if TL_IO_FILE_COMPILER_MSVC
  return (::_fseeki64(file_stream_, offset, posix_whence) == 0);
#else
  static_assert(sizeof(OffsetType) >= sizeof(off_t));
  if (offset >= std::numeric_limits<off_t>::max()) {
    return false;
  }
  return (::fseeko(file_stream_, offset, posix_whence) == 0);
#endif
}

// Semantically it is not const, as the position within the file changes.
// NOLINTNEXTLINE(readability-make-member-function-const)
auto File::Rewind() -> bool { return Seek(0, Whence::kBeginning); }

// Mark as non-const to match other functions.
// NOLINTNEXTLINE(readability-make-member-function-const)
auto File::Tell() -> OffsetType {
#if TL_IO_FILE_COMPILER_MSVC
  return ::_ftelli64(file_stream_);
#else
  static_assert(sizeof(OffsetType) >= sizeof(off_t));
  return ::ftello(file_stream_);
#endif
}

// Uses other method which are not desirable to be marked as const.
// NOLINTNEXTLINE(readability-make-member-function-const)
auto File::Size() -> OffsetType {
  const OffsetType current_offset = Tell();

  Seek(0, Whence::kEnd);

  const OffsetType size = Tell();

  Seek(current_offset, Whence::kBeginning);

  return size;
}

// Semantically it is not const, as the position within the file changes.
// NOLINTNEXTLINE(readability-make-member-function-const)
auto File::Read(void* ptr, const SizeType num_bytes_to_read) -> SizeType {
  SizeType num_bytes_read = 0;

  constexpr size_t kMaxSingleReadSize = std::numeric_limits<size_t>::max();

  auto* cur_ptr = static_cast<uint8_t*>(ptr);

  while (num_bytes_read != num_bytes_to_read) {
    SizeType num_bytes_read_now = num_bytes_to_read - num_bytes_read;
    if (num_bytes_read_now > kMaxSingleReadSize) {
      num_bytes_read_now = kMaxSingleReadSize;
    }

#if TL_IO_FILE_COMPILER_MSVC
    const size_t read_result = ::fread_s(
        cur_ptr, num_bytes_read_now, 1, num_bytes_read_now, file_stream_);
#else
    const size_t read_result =
        ::fread(cur_ptr, 1, num_bytes_read_now, file_stream_);
#endif

    // It is safe to cast since the number of bytes to read was clamped.
    num_bytes_read += SizeType(read_result);

    cur_ptr += read_result;

    if (read_result != num_bytes_read_now) {
      break;
    }
  }

  return num_bytes_read;
}

// Semantically it is not const, as the position within the file changes.
// NOLINTNEXTLINE(readability-make-member-function-const)
auto File::Write(const void* ptr, const SizeType num_bytes_to_write)
    -> SizeType {
  SizeType num_bytes_written = 0;

  constexpr size_t kMaxSingleWriteSize = std::numeric_limits<size_t>::max();

  const auto* cur_ptr = static_cast<const uint8_t*>(ptr);

  while (num_bytes_written != num_bytes_to_write) {
    SizeType num_bytes_written_now = num_bytes_to_write - num_bytes_written;
    if (num_bytes_written_now > kMaxSingleWriteSize) {
      num_bytes_written_now = kMaxSingleWriteSize;
    }

    const size_t write_result =
        ::fwrite(cur_ptr, 1, num_bytes_written_now, file_stream_);

    // It is safe to cast since the number of bytes to write was clamped.
    num_bytes_written += SizeType(write_result);

    cur_ptr += write_result;

    if (write_result != num_bytes_written_now) {
      break;
    }
  }

  return num_bytes_written;
}

inline auto File::IsEOF() -> bool { return ::feof(file_stream_); }

inline auto File::IsError() -> bool { return ::ferror(file_stream_) != 0; }

template <class StringType>
auto File::ReadText(const std::filesystem::path& filename, StringType& text)
    -> bool {
  using CharType = typename StringType::value_type;

  File file;
  if (!file.Open(filename, File::kRead)) {
    return false;
  }

  const OffsetType file_size = file.Size();
  if (file_size == -1) {
    return false;
  }

  if (file_size > text.max_size()) {
    return false;
  }

  // Some file systems like proc or sysfs always report file size of 0, so use
  // the detected size as a hint for the final size, but read in a smaller
  // buffer sizes.

  text.reserve(file_size);

  constexpr size_t kBufferSize = 4096;
  CharType read_buffer[4096];

  while (true) {
    const size_t num_currently_read_bytes = file.Read(read_buffer, kBufferSize);
    if (num_currently_read_bytes == 0) {
      if (file.IsError()) {
        return false;
      }
      break;
    }

    text += std::basic_string_view<CharType>(
        read_buffer, num_currently_read_bytes / sizeof(CharType));

    // If the number of read characters is not a multiple of the character type
    // this is a malformed file which can not be read correctly. So append the
    // information which is available, and return an error.
    if constexpr (sizeof(CharType) > 1) {
      if (num_currently_read_bytes % sizeof(CharType)) {
        return false;
      }
    }
  }

  return true;
}

template <class BufferType>
auto File::ReadBytes(const std::filesystem::path& filename, BufferType& buffer)
    -> bool {
  using ValueType = typename BufferType::value_type;

  File file;
  if (!file.Open(filename, File::kRead)) {
    return false;
  }

  const OffsetType file_size = file.Size();
  if (file_size == -1) {
    return false;
  }

  if (file_size > buffer.max_size()) {
    return false;
  }

  // Some file systems like proc or sysfs always report file size of 0, so use
  // the detected size as a hint for the final size, but read in a smaller
  // buffer sizes.

  buffer.reserve(file_size);

  constexpr size_t kBufferSize = 4096;
  ValueType read_buffer[4096];

  while (true) {
    const size_t num_currently_read_bytes = file.Read(read_buffer, kBufferSize);
    if (num_currently_read_bytes == 0) {
      if (file.IsError()) {
        return false;
      }
      break;
    }

    const std::span<ValueType> new_data{
        read_buffer, num_currently_read_bytes / sizeof(ValueType)};

    buffer.insert(buffer.end(), new_data.begin(), new_data.end());

    // If the number of read characters is not a multiple of the character type
    // this is a malformed file which can not be read correctly. So append the
    // information which is available, and return an error.
    if constexpr (sizeof(ValueType) > 1) {
      if (num_currently_read_bytes % sizeof(ValueType)) {
        return false;
      }
    }
  }

  return true;
}

template <class StringType>
auto File::WriteText(const std::filesystem::path& filename,
                     const StringType& text) -> bool {
  File file;

  if (!file.Open(filename, File::kWrite | File::kCreateAlways)) {
    return false;
  }

  return file.Write(text.data(), text.size()) == text.size();
}

template <class BufferType>
auto File::WriteBytes(const std::filesystem::path& filename,
                      const BufferType& buffer) -> bool {
  File file;

  if (!file.Open(filename, File::kWrite | File::kCreateAlways)) {
    return false;
  }

  return file.Write(buffer.data(), buffer.size()) == buffer.size();
}

}  // namespace TL_IO_FILE_VERSION_NAMESPACE
}  // namespace TL_IO_FILE_NAMESPACE

#undef TL_IO_FILE_VERSION_MAJOR
#undef TL_IO_FILE_VERSION_MINOR
#undef TL_IO_FILE_VERSION_REVISION

#undef TL_IO_FILE_NAMESPACE

#undef TL_IO_FILE_VERSION_NAMESPACE_CONCAT_HELPER
#undef TL_IO_FILE_VERSION_NAMESPACE_CONCAT
#undef TL_IO_FILE_VERSION_NAMESPACE

#undef TL_IO_FILE_COMPILER_MSVC
