// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

// Various string to and from arithmetic conversion utilities.
//
// In contrast with the similar functionality from the standard library these
// conversion utilities do not depend on the current locale and have exact
// matched result on any system configuration. No memory allocation happen in
// these functions, which makes them friendly for embed-able systems with
// limited resources. They might also be faster than the corresponding
// implementation from the standard library.
//
//
// API overview
// ============
//
//  - StringToInt(): Convert string to an integer value.
//  - StringToFloat(): Convert string to a floating point value.
//  - IntToStringBuffer(): Convert integer value to a string and store it in the
//                         given buffer.
//
// Limitations
// ===========
//
//  - Conversion from string expects ASCII string. When an UTF-8 string with
//    multi-byte characters is given each byte is considered separately.
//
// Version history
// ===============
//
//   0.0.1-alpha    (28 Dec 2023)    First public release.

#pragma once

#include <cassert>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

// Semantic version of the tl_convert library.
#define TL_CONVERT_VERSION_MAJOR 0
#define TL_CONVERT_VERSION_MINOR 0
#define TL_CONVERT_VERSION_REVISION 1

// Namespace of the module.
// The outer name spaces which surrounds the ABI-version namespace.
#ifndef TL_CONVERT_NAMESPACE
#  define TL_CONVERT_NAMESPACE tiny_lib::convert
#endif

// Helpers for TL_CONVERT_VERSION_NAMESPACE.
//
// Typical extra indirection for such conversion to allow macro to be expanded
// before it is converted to string.
#define TL_CONVERT_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)              \
  v_##id1##_##id2##_##id3
#define TL_CONVERT_VERSION_NAMESPACE_CONCAT(id1, id2, id3)                     \
  TL_CONVERT_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)

// Constructs identifier suitable for namespace denoting the current library
// version.
//
// For example: TL_CONVERT_VERSION_NAMESPACE -> v_0_1_9
#define TL_CONVERT_VERSION_NAMESPACE                                           \
  TL_CONVERT_VERSION_NAMESPACE_CONCAT(TL_CONVERT_VERSION_MAJOR,                \
                                      TL_CONVERT_VERSION_MINOR,                \
                                      TL_CONVERT_VERSION_REVISION)

// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace TL_CONVERT_NAMESPACE {
inline namespace TL_CONVERT_VERSION_NAMESPACE {

////////////////////////////////////////////////////////////////////////////////
// Public API.

// Convert string to an integer value.
//
// Discards any whitespace characters until the first non-whitespace character
// is found Then the string is being interpreted while the value is
// representing a valid integral.
//
// The optional remainder will hold the un-interpreted part of the string.
// If no valid value was read from the stirng then the remainder matches the
// input string. It is possible to use the input stirng pointer as the
// remainder.
//
// Returns 0 for the empty string input, or there is no valid integer value in
// the stirng after leading whitespace.
//
// Does not perform any locale lookups so often behaves faster than the similar
// functions from the standard library.
//
// Limitations:
//   - Integer and floating point overflows are not handled.
//   - Only base-10 system.
//
// TODO(sergey): Resolve some of the limitations form above without loosing too
// much performance.

template <class IntType>
auto StringToInt(std::string_view str) -> IntType;

template <class IntType>
auto StringToInt(std::string_view str, std::string_view& remainder) -> IntType;

// Convert string to a floating point value.
//
// Discards any whitespace characters until the first non-whitespace character
// is found, Then the string is being interpreted while the value is
// representing a valid floating point value.
//
// The optional remainder will hold the un-interpreted part of the string.
// If no valid value was read from the stirng then the remainder matches the
// input string. It is possible to use the input stirng pointer as the
// remainder.
//
// Returns 0 for the empty string input, or there is no valid floating point
// value in the stirng after leading whitespace.
//
// Does not perform any locale lookups so often behaves faster than the similar
// functions from the standard library.
//
// Limitations:
//   - Integer and floating point overflows are not handled.
//   - Only base-10 system.
//   - Scientific notation is not supported.
//   - nan/inf notation is not supported.
//
// TODO(sergey): Resolve some of the limitations form above without loosing too
// much performance. It will also be good idea to allow API which allows to
// limit the behavior to simple cases only and do not consider scientific or
// nan./inf notations.

template <class RealType>
auto StringToFloat(std::string_view str) -> RealType;

template <class RealType>
auto StringToFloat(std::string_view str, std::string_view& remainder)
    -> RealType;

// Convert integer value to a string and store it in the given buffer.
//
// If the buffer does not have enough space the content of it is undefined and
// the function returns false.
// Upon successful conversion true is returned.
//
// The conversion happens without accessing current system locale, and without
// any memory allocations.
template <class Int>
auto IntToStringBuffer(Int value, std::span<char> buffer) -> bool;

////////////////////////////////////////////////////////////////////////////////
// Implementation.

namespace convert_internal {

// Check whether the given character is considered a whitespace.
// Follows the ASCII rules for the checks: symbols prior to and including space
// are considered whitespace.
inline auto IsWhiteSpace(const char ch) -> bool { return ch <= ' '; }

// Skip any leading whitespace, return the result as a new value.
inline auto SkipLeadingWhitespace(const std::string_view str)
    -> std::string_view {
  const size_t n = str.size();
  size_t i = 0;
  while (i < n) {
    if (!IsWhiteSpace(str[i])) {
      break;
    }
    ++i;
  }
  return str.substr(i);
}

// Convert character to an integer value.
inline auto CharToDigit(const char ch) -> int { return ch - '0'; }

// Check whether the character is a decimal separator.
inline auto IsDecimalSeparator(const char ch) -> bool { return ch == '.'; }

// Implementation of the string to integer conversion.
template <class IntType>
auto StringToInt(const std::string_view str, std::string_view* remainder_ptr)
    -> IntType {
  const std::string_view clean_str = SkipLeadingWhitespace(str);

  size_t index = 0;

  IntType sign = 1;
  if (clean_str.starts_with('-')) {
    sign = -1;
    ++index;
  } else if (clean_str.starts_with('+')) {
    sign = 1;
    ++index;
  }

  const size_t size = clean_str.size();
  IntType result = 0;
  while (index < size) {
    const char ch = clean_str[index];
    if (ch >= '0' && ch <= '9') {
      // TODO(sergey): Find a solution which will avoid per-digit multiplication
      // and will not cause integer overflow for negative values.
      result = result * 10 + CharToDigit(ch) * sign;
      ++index;
    } else {
      break;
    }
  }

  if (remainder_ptr != nullptr) {
    if (index) {
      *remainder_ptr = clean_str.substr(index);
    } else {
      *remainder_ptr = str;
    }
  }

  return result;
}

template <class RealType>
auto StringToFloat(const std::string_view str, std::string_view* remainder_ptr)
    -> RealType {
  const std::string_view clean_str = SkipLeadingWhitespace(str);

  const size_t size = clean_str.size();
  size_t index = 0;

  RealType sign = 1;
  if (clean_str.starts_with('-')) {
    sign = -1;
    ++index;
  } else if (clean_str.starts_with('+')) {
    sign = 1;
    ++index;
  }

  RealType value = 0;

  // Integer part.
  while (index < size) {
    const char ch = clean_str[index];
    if (ch >= '0' && ch <= '9') {
      value = value * 10 + CharToDigit(ch);
      ++index;
    } else {
      break;
    }
  }

  // Fractional part.
  if (index < size && IsDecimalSeparator(clean_str[index])) {
    ++index;
    RealType divider = RealType(1) / 10;
    while (index < size) {
      const char ch = clean_str[index];
      if (ch >= '0' && ch <= '9') {
        value = value + CharToDigit(ch) * divider;
        divider /= 10;
        ++index;
      } else {
        break;
      }
    }
  }

  if (remainder_ptr != nullptr) {
    if (index) {
      *remainder_ptr = clean_str.substr(index);
    } else {
      *remainder_ptr = str;
    }
  }

  return sign * value;
}

// Get absolute value of arg.
// For the unsigned types return value as-is, without extra checks.
template <class Int, std::enable_if_t<std::is_signed_v<Int>, bool> = true>
static auto GetAbsoluteValue(const Int arg) -> Int {
  return arg >= 0 ? arg : -arg;
}
template <class Int, std::enable_if_t<std::is_unsigned_v<Int>, bool> = true>
static auto GetAbsoluteValue(const Int arg) -> Int {
  return arg;
}

// Reverse characters in the given buffer.
inline void Reverse(const std::span<char> buffer) {
  if (buffer.empty()) {
    return;
  }

  char* left = buffer.data();
  char* right = buffer.data() + buffer.size() - 1;
  while (left < right) {
    std::swap(*left, *right);
    ++left;
    --right;
  }
}

}  // namespace convert_internal

template <class IntType>
auto StringToInt(const std::string_view str) -> IntType {
  return convert_internal::StringToInt<IntType>(str, nullptr);
}

template <class IntType>
auto StringToInt(const std::string_view str, std::string_view& remainder)
    -> IntType {
  return convert_internal::StringToInt<IntType>(str, &remainder);
}

template <class RealType>
auto StringToFloat(const std::string_view str) -> RealType {
  return convert_internal::StringToFloat<RealType>(str, nullptr);
}

template <class RealType>
auto StringToFloat(const std::string_view str, std::string_view& remainder)
    -> RealType {
  return convert_internal::StringToFloat<RealType>(str, &remainder);
}

template <class Int>
auto IntToStringBuffer(const Int value, std::span<char> buffer) -> bool {
  if (buffer.empty()) {
    return false;
  }

  const size_t buffer_size = buffer.size();
  size_t num_chars_written = 0;

  // Convert positive value to the buffer.
  Int value_convert = convert_internal::GetAbsoluteValue(value);
  do {
    const int digit = value_convert % 10;
    value_convert /= 10;

    if (num_chars_written == buffer_size) {
      return false;
    }

    buffer[num_chars_written++] = char('0' + digit);
  } while (value_convert != 0);

  if (value < 0) {
    if (num_chars_written == buffer_size) {
      return false;
    }
    buffer[num_chars_written++] = '-';
  }

  convert_internal::Reverse(buffer.subspan(0, num_chars_written));

  // Append null-terminator.
  if (num_chars_written == buffer_size) {
    return false;
  }
  buffer[num_chars_written++] = '\0';

  return true;
}

}  // namespace TL_CONVERT_VERSION_NAMESPACE
}  // namespace TL_CONVERT_NAMESPACE

#undef TL_CONVERT_VERSION_MAJOR
#undef TL_CONVERT_VERSION_MINOR
#undef TL_CONVERT_VERSION_REVISION

#undef TL_CONVERT_NAMESPACE

#undef TL_CONVERT_VERSION_NAMESPACE_CONCAT_HELPER
#undef TL_CONVERT_VERSION_NAMESPACE_CONCAT
#undef TL_CONVERT_VERSION_NAMESPACE
