// Copyright (c) 2023 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

// An automation of creation and life management of temporary files.
// The temporary file is created upon call of TempFile::Open() and is deleted
// when the object is destructed.
//
//
// Example
// =======
//
//   if (TempFile temp_file; temp_file.Open()) {
//     std::cout << temp_file.GetFilePath() << std::endl;
//
//     FILE* stream = temp_file.GetStream();
//     fprintf(stream, "Hello, World!\n");
//   }
//
//
// Limitations
// ===========
//
//  - Temp file might br not removed upon application crash.
//
//
// Version history
// ===============
//
//   0.0.1-alpha    (28 Dec 2023)    First public release.

#pragma once

#include <cstdio>

#include <filesystem>
#include <random>
#include <string>
#include <string_view>

#include <fcntl.h>

#if defined(__APPLE__)
#  define TL_TEMP_FILE_OS_MACOSX 1
#else
#  define TL_TEMP_FILE_OS_MACOSX 0
#endif

#if defined(_MSC_VER)
#  define TL_TEMP_FILE_COMPILER_MSVC 1
#else
#  define TL_TEMP_FILE_COMPILER_MSVC 0
#endif

#if TL_TEMP_FILE_COMPILER_MSVC
#  include <io.h>
#  ifndef NOGDI
#    define NOGDI
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOCOMM
#    define NOCOMM
#  endif
#  include <windows.h>
#else
#  include <sys/param.h>
#  include <unistd.h>
#  if !TL_TEMP_FILE_OS_MACOSX
#    include <unistd.h>
#  endif
#endif

// Semantic version of the tl_temp_file library.
#define TL_TEMP_FILE_VERSION_MAJOR 0
#define TL_TEMP_FILE_VERSION_MINOR 0
#define TL_TEMP_FILE_VERSION_REVISION 1

// Namespace of the module.
// The outer name spaces which surrounds the ABI-version namespace.
#ifndef TL_TEMP_FILE_NAMESPACE
#  define TL_TEMP_FILE_NAMESPACE tiny_lib::temp_file
#endif

// Helpers for TL_TEMP_FILE_VERSION_NAMESPACE.
//
// Typical extra indirection for such conversion to allow macro to be expanded
// before it is converted to string.
#define TL_TEMP_FILE_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)            \
  v_##id1##_##id2##_##id3
#define TL_TEMP_FILE_VERSION_NAMESPACE_CONCAT(id1, id2, id3)                   \
  TL_TEMP_FILE_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)

// Constructs identifier suitable for namespace denoting the current library
// version.
//
// For example: TL_TEMP_FILE_VERSION_NAMESPACE -> v_0_1_9
#define TL_TEMP_FILE_VERSION_NAMESPACE                                         \
  TL_TEMP_FILE_VERSION_NAMESPACE_CONCAT(TL_TEMP_FILE_VERSION_MAJOR,            \
                                        TL_TEMP_FILE_VERSION_MINOR,            \
                                        TL_TEMP_FILE_VERSION_REVISION)

// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace TL_TEMP_FILE_NAMESPACE {
inline namespace TL_TEMP_FILE_VERSION_NAMESPACE {

////////////////////////////////////////////////////////////////////////////////
// Public API declaration.

class TempFile {
 public:
  // Closes any open file descriptor which is still open.
  inline ~TempFile();

  // Open temporary file for write.
  //
  // The optional prefix and suffix arguments denotes prefix and suffix of the
  // file name generated and saved on disk. The middle part (between the
  // suffix and the prefix) will contain a randomized character sequence.
  //
  // Returns true on success.
  inline auto Open(const std::string_view prefix = "",
                   const std::string_view suffix = "") -> bool;

  // Close all open file descriptors held by the TempFile.
  inline void Close();

  // Get full path to this temporary file.
  // If the temp file is not yet open an empty path is returned.
  inline auto GetFilePath() const -> std::filesystem::path;

  // Get file stream associated with this temp file.
  inline auto GetStream() -> FILE* { return stream_; }

 private:
  FILE* stream_ = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
// Implementation.

namespace internal {

using Generator = std::mt19937;

// Abstraction of a file descriptor which allows to have the majority of the
// algorithm shared across different platforms.
// The file descriptor is used by the low-level platform specific function
// which opens file in an exclusive creation manner.
using FileDescriptor = int;
static inline FileDescriptor kInvalidFileDescriptor = -1;

// Create generator which is used to generate random file names.
// The generator is seeded in a way that simultaneous request of random file
// names from different thread is seeded differently.
inline auto CreateSeededGenerator() -> Generator {
  std::random_device random_device;
  return std::mt19937(random_device());
}

// Generate random file name, with high probability of being unique.
// The actual uniqueness is not guaranteed by this function.
inline auto GenerateRandomFileName(Generator& generator,
                                   const std::string_view prefix,
                                   const std::string_view suffix)
    -> std::string {
  static const char kLetters[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  static const int kNumLetters = sizeof(kLetters) / sizeof(char) - 1;

  std::uniform_int_distribution<int> uniform_dist(0, kNumLetters - 1);

  std::string file_name(prefix);

  for (int i = 0; i < 16; ++i) {
    file_name += kLetters[uniform_dist(generator)];
  }

  file_name += std::string(suffix);

  return file_name;
}

// Open file descriptor for the given file path.
// The valid file descriptor is returned only if the file did not exist before
// opening it.
inline auto CreateAndOpenFileDescriptor(const std::filesystem::path& file_path)
    -> FileDescriptor {
  const int flags = O_RDWR     // Open for reading and writing.
                    | O_CREAT  // Create file if it does not exist.
                    | O_EXCL   // Error if O_CREAT and the file exists.
      ;

#if TL_TEMP_FILE_COMPILER_MSVC
  int file_descriptor = -1;
  ::_sopen_s(&file_descriptor,
             file_path.generic_string().c_str(),
             flags,
             _SH_DENYRW,
             _S_IREAD | _S_IWRITE);
  return file_descriptor;
#else
  return ::open(file_path.generic_string().c_str(), flags, 0600);
#endif
}

// Open given file descriptor as FILE*.
inline auto OpenFileDescriptorAsStream(const FileDescriptor& fd) -> FILE* {
#if TL_TEMP_FILE_COMPILER_MSVC
  return ::_fdopen(fd, "r+b");
#else
  return ::fdopen(fd, "r+b");
#endif
}

#if TL_TEMP_FILE_COMPILER_MSVC
inline auto GetFileStreamPath(FILE* stream) -> std::filesystem::path {
  if (!stream) {
    return {};
  }

  const int fd = ::_fileno(stream);
  HANDLE file_handle = HANDLE(_get_osfhandle(fd));
  if (file_handle == INVALID_HANDLE_VALUE) {
    return {};
  }

  const DWORD required_size =
      ::GetFinalPathNameByHandleW(file_handle, nullptr, 0, VOLUME_NAME_DOS);
  if (required_size == 0) {
    return {};
  }

  std::wstring filename(required_size - 1, L'X');
  const DWORD actual_size = ::GetFinalPathNameByHandleW(
      file_handle, filename.data(), required_size, VOLUME_NAME_DOS);
  (void)actual_size;

  return {filename};
}
#elif TL_TEMP_FILE_OS_MACOSX
inline auto GetFileStreamPath(FILE* stream) -> std::filesystem::path {
  if (!stream) {
    return {};
  }

  const int fd = ::fileno(stream);

  char path[MAXPATHLEN];
  if (::fcntl(fd, F_GETPATH, path) != 0) {
    return {};
  }

  return {path};
}
#else
inline auto GetFileStreamPath(FILE* stream) -> std::filesystem::path {
  if (!stream) {
    return {};
  }

  const int fd = ::fileno(stream);

  char path[MAXPATHLEN];

  const std::string fd_path = "/proc/self/fd/" + std::to_string(fd);
  const ssize_t num_bytes = readlink(fd_path.c_str(), path, sizeof(path));

  if (num_bytes == -1) {
    return {};
  }

  return {path};
}
#endif

}  // namespace internal

inline TempFile::~TempFile() { Close(); }

inline auto TempFile::Open(const std::string_view prefix,
                           const std::string_view suffix) -> bool {
  namespace filesystem = std::filesystem;

  // Get path to the current temp directory.
  // For example, typically on Linux it will be /tmp.
  std::error_code error_code;
  const filesystem::path temp_dir = filesystem::temp_directory_path(error_code);
  if (bool(error_code) || temp_dir.empty()) {
    return false;
  }

  // Generator which is used to generate random file names.
  internal::Generator generator = internal::CreateSeededGenerator();

  // The counter of attemps to generate a random file name and open it for
  // write in an exclusive manner. The algorithm will give up after this many
  // attempts and return false.
  int counter = 32768;

  do {
    const std::string file_name =
        internal::GenerateRandomFileName(generator, prefix, suffix);

    const filesystem::path file_path = temp_dir / file_name;

    const internal::FileDescriptor fd =
        internal::CreateAndOpenFileDescriptor(file_path);
    if (fd == internal::kInvalidFileDescriptor) {
      --counter;
      continue;
    }

    // Open buffered stream for the file communication.
    // The file descriptor is associated with the stream and will be closed
    // upon fclose(stream_).
    stream_ = internal::OpenFileDescriptorAsStream(fd);

  } while (!stream_ && counter >= 0);

  return stream_ != nullptr;
}

void TempFile::Close() {
  if (stream_) {
    const std::filesystem::path path = GetFilePath();

    ::fclose(stream_);
    stream_ = nullptr;

    if (!path.empty()) {
      std::error_code error_code;
      std::filesystem::remove(path, error_code);
    }
  }
}

inline auto TempFile::GetFilePath() const -> std::filesystem::path {
  return internal::GetFileStreamPath(stream_);
}

}  // namespace TL_TEMP_FILE_VERSION_NAMESPACE
}  // namespace TL_TEMP_FILE_NAMESPACE

#undef TL_TEMP_FILE_VERSION_MAJOR
#undef TL_TEMP_FILE_VERSION_MINOR
#undef TL_TEMP_FILE_VERSION_REVISION

#undef TL_TEMP_FILE_NAMESPACE

#undef TL_TEMP_FILE_VERSION_NAMESPACE_CONCAT_HELPER
#undef TL_TEMP_FILE_VERSION_NAMESPACE_CONCAT
#undef TL_TEMP_FILE_VERSION_NAMESPACE

#undef TL_TEMP_FILE_COMPILER_MSVC
#undef TL_TEMP_FILE_OS_MACOSX
