// Copyright (c) 2021 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

// Versions of POSIX string functions which ensures predictable behavior in
// terms of null-terminator handling and return values on all platforms and
// compilers.
//
// Additionally, using these functions also takes care of the naming mismatch
// for the safe MSVC function naming.
//
// NOTE: This is an adaptation of an old code, which actually pre-dates C++11.
// The more modern C++ standards are much better aligned in the behavior
// (although, maybe not in naming).
//
// Version history
// ---------------
//
//   0.0.3-alpha    (24 Nov 2024)    Return the number of characters copied from
//                                   the strncpy().
//   0.0.2-alpha    (29 Dec 2023)    Allow snprintf() to receive nullptr buffer.
//   0.0.1-alpha    (28 Dec 2023)    First public release.

#pragma once

#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>

// Semantic version of the tl_string_portable library.
#define TL_STRING_PORTABLE_VERSION_MAJOR 0
#define TL_STRING_PORTABLE_VERSION_MINOR 0
#define TL_STRING_PORTABLE_VERSION_REVISION 2

// Namespace of the module.
// The outer name spaces which surrounds the ABI-version namespace.
#ifndef TL_STRING_PORTABLE_NAMESPACE
#  define TL_STRING_PORTABLE_NAMESPACE tiny_lib::string_portable
#endif

// Helpers for TL_STRING_PORTABLE_VERSION_NAMESPACE.
//
// Typical extra indirection for such conversion to allow macro to be expanded
// before it is converted to string.
#define TL_STRING_PORTABLE_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)      \
  v_##id1##_##id2##_##id3
#define TL_STRING_PORTABLE_VERSION_NAMESPACE_CONCAT(id1, id2, id3)             \
  TL_STRING_PORTABLE_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)

// Constructs identifier suitable for namespace denoting the current library
// version.
//
// For example: TL_STRING_PORTABLE_VERSION_NAMESPACE -> v_0_1_9
#define TL_STRING_PORTABLE_VERSION_NAMESPACE                                   \
  TL_STRING_PORTABLE_VERSION_NAMESPACE_CONCAT(                                 \
      TL_STRING_PORTABLE_VERSION_MAJOR,                                        \
      TL_STRING_PORTABLE_VERSION_MINOR,                                        \
      TL_STRING_PORTABLE_VERSION_REVISION)

// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace TL_STRING_PORTABLE_NAMESPACE {
inline namespace TL_STRING_PORTABLE_VERSION_NAMESPACE {

// Portable version of `::strncpy` which ensure the result is always
// null-terminated as long as the destination has a non-zero size.
//
// Copies characters from a null-terminated `src` to `dst`. At maximum of
// `dst_size` will be written to the `dst` (including the null-terminator).
//
// The naming follows the standard C library naming.
//
// Returns the number of characters written to the destination, excluding the
// null-terminator.
//
// NOTE: The source and destination must not alias.
//
// NOLINTNEXTLINE(readability-identifier-naming)
inline auto strncpy(char* dst, const char* src, const size_t dst_size)
    -> size_t {
  if (dst_size == 0) {
    return 0;
  }

  const size_t src_len = ::strlen(src);
  const size_t len_to_copy = std::min(src_len, dst_size - 1);
  ::memcpy(dst, src, len_to_copy);
  dst[len_to_copy] = '\0';

  return len_to_copy;
}

// Portable version of `::vsnprintf()` which ensures the result is always
// null-terminated.
//
// Return the number of characters that would have been printed if the buffer
// size was unlimited, not including the null-terminator.
//
// The buffer can be set to nullptr and the buffer_size to zero. In this case
// the function returns the number of characters needed in the buffer to fit the
// requested format.
//
// The naming follows the standard C library naming.
// NOLINTNEXTLINE(readability-identifier-naming)
inline auto vsnprintf(char* buffer,
                      size_t buffer_size,
                      const char* format,
                      va_list arg) -> int {
  assert(format != nullptr);

  int num_printed = ::vsnprintf(buffer, buffer_size, format, arg);

#if defined(_MSC_VER)
  // Windows return -1 when calling vsnprintf with arguments that exceed target
  // buffer size. This API return -1 while other platforms (linux, bsd) return
  // 10 instead. Other platform's reference can be found below.
  //
  // See https://bugs.webkit.org/show_bug.cgi?id=140917
  if (num_printed < 0 && errno != EINVAL) {
    num_printed = ::_vscprintf(format, arg);
  }
#endif

  if (buffer) {
    if (num_printed != -1 && num_printed < buffer_size) {
      buffer[num_printed] = '\0';
    } else {
      buffer[buffer_size - 1] = '\0';
    }
  }

  return num_printed;
}

// Portable version of `::snprintf()` which ensures the result is always
// null-terminated.
//
// Return the number of characters that would have been printed if the buffer
// size was unlimited, not including the null-terminator.
//
// The naming follows the standard C library naming.
// NOLINTNEXTLINE(readability-identifier-naming)
inline auto snprintf(char* buffer, size_t buffer_size, const char* format, ...)
    -> int {
  va_list arg;
  va_start(arg, format);
  const int num_printed = vsnprintf(buffer, buffer_size, format, arg);
  va_end(arg);
  return num_printed;
}

}  // namespace TL_STRING_PORTABLE_VERSION_NAMESPACE
}  // namespace TL_STRING_PORTABLE_NAMESPACE

#undef TL_STRING_PORTABLE_VERSION_MAJOR
#undef TL_STRING_PORTABLE_VERSION_MINOR
#undef TL_STRING_PORTABLE_VERSION_REVISION

#undef TL_STRING_PORTABLE_NAMESPACE

#undef TL_STRING_PORTABLE_VERSION_NAMESPACE_CONCAT_HELPER
#undef TL_STRING_PORTABLE_VERSION_NAMESPACE_CONCAT
#undef TL_STRING_PORTABLE_VERSION_NAMESPACE
