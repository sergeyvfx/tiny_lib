// Copyright (c) 2023 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

// An automation of creation and life management of temporary directories.
// The temporary directory is created upon call of TempDir::Open() and is
// deleted when the object is destructed.
//
//
// Example
// =======
//
//   if (TempDir temp_dir; temp_dir.Open()) {
//     std::cout << temp_dir.GetPath() << std::endl;
//   }
//
//
// Limitations
// ===========
//
//  - Temp directory might br not removed upon application crash.
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

// Semantic version of the tl_temp_dir library.
#define TL_TEMP_DIR_VERSION_MAJOR 0
#define TL_TEMP_DIR_VERSION_MINOR 0
#define TL_TEMP_DIR_VERSION_REVISION 1

// Namespace of the module.
// The outer name spaces which surrounds the ABI-version namespace.
#ifndef TL_TEMP_DIR_NAMESPACE
#  define TL_TEMP_DIR_NAMESPACE tiny_lib::temp_dir
#endif

// Helpers for TL_TEMP_DIR_VERSION_NAMESPACE.
//
// Typical extra indirection for such conversion to allow macro to be expanded
// before it is converted to string.
#define TL_TEMP_DIR_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)             \
  v_##id1##_##id2##_##id3
#define TL_TEMP_DIR_VERSION_NAMESPACE_CONCAT(id1, id2, id3)                    \
  TL_TEMP_DIR_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)

// Constructs identifier suitable for namespace denoting the current library
// version.
//
// For example: TL_TEMP_DIR_VERSION_NAMESPACE -> v_0_1_9
#define TL_TEMP_DIR_VERSION_NAMESPACE                                          \
  TL_TEMP_DIR_VERSION_NAMESPACE_CONCAT(TL_TEMP_DIR_VERSION_MAJOR,              \
                                       TL_TEMP_DIR_VERSION_MINOR,              \
                                       TL_TEMP_DIR_VERSION_REVISION)

// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace TL_TEMP_DIR_NAMESPACE {
inline namespace TL_TEMP_DIR_VERSION_NAMESPACE {

////////////////////////////////////////////////////////////////////////////////
// Public API declaration.

class TempDir {
 public:
  // Deletes any remaining files in the temp directory and the directory itself.
  inline ~TempDir();

  // Create temporary directory.
  //
  // The optional prefix and suffix arguments denotes prefix and suffix of the
  // generated directory name. The middle part (between the suffix and the
  // prefix) will contain a randomized character sequence.
  //
  // Returns true on success.
  inline auto Open(const std::string_view prefix = "",
                   const std::string_view suffix = "") -> bool;

  // Delete any remaining files in the temp directory and the directory itself.
  inline void Close();

  // Get full path to this temporary directory.
  // If the temp directory is not yet open an empty path is returned.
  inline auto GetPath() const -> const std::filesystem::path&;

 private:
  std::filesystem::path path_;
};

////////////////////////////////////////////////////////////////////////////////
// Implementation.

namespace internal {

using Generator = std::mt19937;

// Create generator which is used to generate random directory names.
// The generator is seeded in a way that simultaneous request of random
// directory names from different thread is seeded differently.
inline auto CreateSeededGenerator() -> Generator {
  std::random_device random_device;
  return std::mt19937(random_device());
}

// Generate random directory name, with high probability of being unique.
// The actual uniqueness is not guaranteed by this function.
inline auto GenerateRandomDirectoryName(Generator& generator,
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

}  // namespace internal

inline TempDir::~TempDir() { Close(); }

inline auto TempDir::Open(const std::string_view prefix,
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

  // The counter of attemps to generate a random file name and open it for write
  // in an exclusive manner. The algorithm will give up after this many attempts
  // and return false.
  int counter = 32768;

  do {
    const std::string dir_name =
        internal::GenerateRandomDirectoryName(generator, prefix, suffix);
    const filesystem::path path = temp_dir / dir_name;

    // Creating an existing directory returns false.
    if (std::filesystem::create_directory(path)) {
      path_ = path;
      return true;
    }
    --counter;
  } while (counter >= 0);

  return false;
}

void TempDir::Close() {
  if (!path_.empty()) {
    std::error_code error_code;
    std::filesystem::remove_all(path_, error_code);

    path_ = std::filesystem::path();
  }
}

inline auto TempDir::GetPath() const -> const std::filesystem::path& {
  return path_;
}

}  // namespace TL_TEMP_DIR_VERSION_NAMESPACE
}  // namespace TL_TEMP_DIR_NAMESPACE

#undef TL_TEMP_DIR_VERSION_MAJOR
#undef TL_TEMP_DIR_VERSION_MINOR
#undef TL_TEMP_DIR_VERSION_REVISION

#undef TL_TEMP_DIR_NAMESPACE

#undef TL_TEMP_DIR_VERSION_NAMESPACE_CONCAT_HELPER
#undef TL_TEMP_DIR_VERSION_NAMESPACE_CONCAT
#undef TL_TEMP_DIR_VERSION_NAMESPACE
