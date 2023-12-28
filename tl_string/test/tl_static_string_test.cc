// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#include "tl_string/tl_static_string.h"

#include <array>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

#include "tiny_lib/unittest/test.h"

// When it is defined to 1 the test will run tests against the std::string.
// This is used to align the behavior of the StaticString with the STL.
#define USE_STD_STRING_REFERENCE 0

// The compiler might not detect that the code checks for access past the array
// bounds and throws an exception is such case. This makes it hard to write test
// for this specific case without triggering verbose warning.
//
// Surrounding the call which tests for access past the array bounds with the
// IGNORE_ARRAY_BOUNDS_BEGIN/IGNORE_ARRAY_BOUNDS_END will silence the compiler
// warning.
#ifdef __GNUC__
#  define IGNORE_ARRAY_BOUNDS_BEGIN                                            \
    _Pragma("GCC diagnostic push");                                            \
    _Pragma("GCC diagnostic ignored \"-Warray-bounds\"");
#  define IGNORE_ARRAY_BOUNDS_END _Pragma("GCC diagnostic pop");
#else
#  define IGNORE_ARRAY_BOUNDS_BEGIN
#  define IGNORE_ARRAY_BOUNDS_END
#endif

namespace tiny_lib::static_string {

// Wrap the test into its own namespace, making it easy to define local
// StaticString class which is an alias to std::string.
namespace test {

#if USE_STD_STRING_REFERENCE
template <int N>
using StaticString = std::string;

using std::erase;
using std::erase_if;

inline auto AsView(const std::string& str) -> std::string_view { return str; }
#else
// Convert the StaticString to a view using the lowest level possible API.
template <std::size_t N>
inline auto AsView(const StaticString<N>& str) -> std::string_view {
  return std::string_view(str.data(), str.size());
}
#endif

// helper macro to test whether StaticString str matches the expected string.
// Performs checks of both content and null-terminator.
#define EXPECT_STATIC_STR_EQ(str, expected)                                    \
  do {                                                                         \
    std::remove_reference_t<decltype(str)> local_str{str};                     \
    EXPECT_EQ(AsView(local_str), expected);                                    \
    EXPECT_EQ(local_str.data()[local_str.size()], '\0');                       \
  } while (false)

////////////////////////////////////////////////////////////////////////////////
// Member functions.

TEST(tl_static_string, Construct) {
  // Default constructor

  {
    StaticString<10> str;
    EXPECT_STATIC_STR_EQ(str, "");
    EXPECT_EQ(str.size(), 0);
  }

  // ### String with count copies of a character

  {
    StaticString<10> str(3, 'w');
    EXPECT_STATIC_STR_EQ(str, "www");
    EXPECT_EQ(str.size(), 3);
  }

  // Number of characters matches the requested capacity.
  {
    StaticString<10> str(10, 'w');
    EXPECT_STATIC_STR_EQ(str, "wwwwwwwwww");
    EXPECT_EQ(str.size(), 10);
  }

  // Construct from a count copies of a character exceeding the maximum
  // capacity.
  if (!USE_STD_STRING_REFERENCE) {
    IGNORE_ARRAY_BOUNDS_BEGIN
    EXPECT_THROW_OR_ABORT(StaticString<10>(11, 'w'), std::length_error);
    IGNORE_ARRAY_BOUNDS_END
  }

  // ### Construct a substring

  // Construct from a substring using npos as a count.
  {
    const StaticString<10> src("abcdef");
    StaticString<10> str(src, 1);
    EXPECT_STATIC_STR_EQ(str, "bcdef");
    EXPECT_EQ(str.size(), 5);
  }

  // Construct from a substring using a fixed small count.
  {
    const StaticString<10> src("abcdef");
    StaticString<10> str(src, 1, 2);
    EXPECT_STATIC_STR_EQ(str, "bc");
    EXPECT_EQ(str.size(), 2);
  }

  // Construct from a substring with position past the source size.
  {
    const StaticString<10> src(3, 'w');
    EXPECT_THROW_OR_ABORT(StaticString<10>(src, 10), std::out_of_range);
  }

  // ### Move constructor

  {
    StaticString<10> src("abcdef");
    StaticString<10> str(std::move(src));
    EXPECT_STATIC_STR_EQ(str, "abcdef");
    EXPECT_EQ(str.size(), 6);
  }

  // ### Construct from characters in a range

  // Simple case: substring of a static string.
  // Ensures that is not the entire length is copied.
  {
    StaticString<10> str("abcdef", 3);
    EXPECT_STATIC_STR_EQ(str, "abc");
    EXPECT_EQ(str.size(), 3);
  }

  // Validate the behavior with null terminators in a range.
  {
    StaticString<10> str("abc\0def", 6);
    EXPECT_STATIC_STR_EQ(str, std::string_view("abc\0de", 6));
    EXPECT_EQ(str.size(), 6);
  }

  // Character sequence length matches the maximum string capacity.
  {
    StaticString<6> str("abcdef", 6);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
    EXPECT_EQ(str.size(), 6);
  }

  // Character sequence length exceeds the maximum string capacity.
  if (!USE_STD_STRING_REFERENCE) {
    EXPECT_THROW_OR_ABORT(StaticString<5>("abcdef", 6), std::length_error);
  }

  // ### Construct from a null-terminated sequence of characters

  {
    StaticString<10> str("abcdef");
    EXPECT_STATIC_STR_EQ(str, "abcdef");
    EXPECT_EQ(str.size(), 6);
  }

  // Validate the behavior with null terminators in the range.
  {
    StaticString<10> str("abc\0def");
    EXPECT_STATIC_STR_EQ(str, "abc");
    EXPECT_EQ(str.size(), 3);
  }

  // Character sequence length matches the maximum string capacity.
  {
    StaticString<6> str("abcdef");
    EXPECT_STATIC_STR_EQ(str, "abcdef");
    EXPECT_EQ(str.size(), 6);
  }

  // Character sequence length exceeds the maximum string capacity.
  if (!USE_STD_STRING_REFERENCE) {
    EXPECT_THROW_OR_ABORT(StaticString<5>("abcdef"), std::length_error);
  }

  // ### Construct from an iterator

  {
    const std::string src("abcdef");
    StaticString<10> str(src.begin(), src.end());
    EXPECT_STATIC_STR_EQ(str, "abcdef");
    EXPECT_EQ(str.size(), 6);
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    const std::string src("abcdef");
    EXPECT_THROW_OR_ABORT(StaticString<5>(src.begin(), src.end()),
                          std::length_error);
  }

  // ### Construct from an initializer list

  {
    StaticString<10> str({'C', '-', 's', 't', 'y', 'l', 'e'});
    EXPECT_STATIC_STR_EQ(str, "C-style");
    EXPECT_EQ(str.size(), 7);
  }

  // Length of the initializer list matches the maximum string capacity.
  {
    StaticString<7> str({'C', '-', 's', 't', 'y', 'l', 'e'});
    EXPECT_STATIC_STR_EQ(str, "C-style");
    EXPECT_EQ(str.size(), 7);
  }

  // Length of the initializer list exceeds the maximum string capacity.
  if (!USE_STD_STRING_REFERENCE) {
    IGNORE_ARRAY_BOUNDS_BEGIN
    EXPECT_THROW_OR_ABORT(StaticString<6>({'C', '-', 's', 't', 'y', 'l', 'e'}),
                          std::length_error);
    IGNORE_ARRAY_BOUNDS_END
  }

  // ### Construct from convertible to a string view

  {
    StaticString<10> str(std::string("abcdef"));
    EXPECT_STATIC_STR_EQ(str, "abcdef");
    EXPECT_EQ(str.size(), 6);
  }

  // Length of the source matches the maximum capacity.
  {
    StaticString<12> str(std::string("Hello, World"));
    EXPECT_STATIC_STR_EQ(str, "Hello, World");
    EXPECT_EQ(str.size(), 12);
  }

  // Length of the source exceeds the maximum capacity.
  if (!USE_STD_STRING_REFERENCE) {
    EXPECT_THROW_OR_ABORT(StaticString<11>(std::string("Hello, World")),
                          std::length_error);
  }

  // ### Assign a substring of a view-like object

  // Copy npos count.
  {
    const StaticString<10> str(std::string("abcdef"), 1);
    EXPECT_STATIC_STR_EQ(str, "bcdef");
    EXPECT_EQ(str.size(), 5);
  }

  // Copy fixed small count.
  {
    const StaticString<10> str(std::string("abcdef"), 1, 2);
    EXPECT_STATIC_STR_EQ(str, "bc");
    EXPECT_EQ(str.size(), 2);
  }

  // Assign substring with source position past the source size.
  {
    const std::string src(3, 'w');
    EXPECT_THROW_OR_ABORT(StaticString<10>(src, 10), std::out_of_range);
  }

  // Length of the source matches the maximum capacity.
  {
    StaticString<4> str(std::string("Hello, World", 1, 4));
    EXPECT_STATIC_STR_EQ(str, "ello");
    EXPECT_EQ(str.size(), 4);
  }

  // Length of the source exceeds the maximum capacity.
  if (!USE_STD_STRING_REFERENCE) {
    EXPECT_THROW_OR_ABORT(StaticString<3>(std::string("Hello, World", 1, 4)),
                          std::length_error);
  }
}

TEST(tl_static_string, AssignValue) {
  // ### Replace the contents with a copy of str

  {
    const StaticString<10> src("Hello");
    StaticString<10> str;
    str = src;
    EXPECT_STATIC_STR_EQ(str, "Hello");
  }

  // ### Replace the contents with those of str using move semantics

  {
    StaticString<10> src("Hello");
    StaticString<10> str;
    str = std::move(src);
    EXPECT_STATIC_STR_EQ(str, "Hello");
  }

  // ### Replace the contents with those of null-terminated character string

  {
    StaticString<10> str;
    str = "Hello";
    EXPECT_STATIC_STR_EQ(str, "Hello");
  }

  // Length of characters matches capacity of the string.
  {
    StaticString<5> str;
    str = "Hello";
    EXPECT_STATIC_STR_EQ(str, "Hello");
  }

  // Length of characters exceeds capacity of the string.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<4> str("old");
    EXPECT_THROW_OR_ABORT(str = "Hello", std::length_error);
    EXPECT_STATIC_STR_EQ(str, "old");
  }

  // ### Replace the contents with a character

  {
    StaticString<10> str;
    str = 'w';
    EXPECT_STATIC_STR_EQ(str, "w");
  }

  // ### Replace the contents with those of the initializer list

  {
    StaticString<10> str;
    str = {'C', '-', 's', 't', 'y', 'l', 'e'};
    EXPECT_STATIC_STR_EQ(str, "C-style");
  }

  // Size of thr list matches the string capacity.
  {
    StaticString<7> str;
    str = {'C', '-', 's', 't', 'y', 'l', 'e'};
    EXPECT_STATIC_STR_EQ(str, "C-style");
  }

  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("old");
    const std::initializer_list<char> ilist = {
        'C', '-', 's', 't', 'y', 'l', 'e'};
    EXPECT_THROW_OR_ABORT(str = ilist, std::length_error);
    EXPECT_STATIC_STR_EQ(str, "old");
  }

  // ### Assign a string_view-like value

  {
    StaticString<10> str;
    str = std::string("Hello");
    EXPECT_STATIC_STR_EQ(str, "Hello");
  }

  // View length matches the capacity of the string.
  {
    StaticString<5> str;
    str = std::string("Hello");
    EXPECT_STATIC_STR_EQ(str, "Hello");
  }

  // View length exceeds the capacity of the string.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<4> str("old");
    EXPECT_THROW_OR_ABORT(str = std::string("Hello"), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "old");
  }
}

TEST(tl_static_string, AssignCharacters) {
  // ### Assign count copies of a character

  // Small case.
  {
    StaticString<10> str;
    str.assign(3, 'w');
    EXPECT_STATIC_STR_EQ(str, "www");
  }

  // Number of characters matches the requested capacity.
  {
    StaticString<10> str;
    str.assign(10, 'w');
    EXPECT_STATIC_STR_EQ(str, "wwwwwwwwww");
  }

  // Assign count copies of a character exceeding the maximum capacity.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<10> str("old");
    IGNORE_ARRAY_BOUNDS_BEGIN
    EXPECT_THROW_OR_ABORT(str.assign(11, 'w'), std::length_error);
    IGNORE_ARRAY_BOUNDS_END
    EXPECT_STATIC_STR_EQ(str, "old");
  }

  // ### Assign a copy of string

  {
    const StaticString<10> src(3, 'w');
    StaticString<10> str;
    str.assign(src);
    EXPECT_STATIC_STR_EQ(str, "www");
  }

  // ### Assign a substring

  // Copy npos count.
  {
    const StaticString<10> src("abcdef");
    StaticString<10> str;
    str.assign(src, 1);
    EXPECT_STATIC_STR_EQ(str, "bcdef");
  }

  // Copy fixed small count.
  {
    const StaticString<10> src("abcdef");
    StaticString<10> str;
    str.assign(src, 1, 2);
    EXPECT_STATIC_STR_EQ(str, "bc");
  }

  // Assign substring with source position past the source size.
  {
    const StaticString<10> src(3, 'w');
    StaticString<10> str("old");
    EXPECT_THROW_OR_ABORT(str.assign(src, 10), std::out_of_range);
    EXPECT_STATIC_STR_EQ(str, "old");
  }

  // ### Move assignment

  {
    StaticString<10> src("abcdef");
    StaticString<10> str;
    str.assign(std::move(src));
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // ### Assign a range of characters

  // Simple case: substring of a static string.
  // Ensures that is not the entire length is copied.
  {
    StaticString<10> str;
    str.assign("abcdef", 3);
    EXPECT_STATIC_STR_EQ(str, "abc");
  }

  // Validate the behavior with null terminators in the range.
  {
    StaticString<10> str;
    str.assign("abc\0def", 6);
    EXPECT_STATIC_STR_EQ(str, std::string_view("abc\0de", 6));
  }

  // Character sequence length matches the maximum string capacity.
  {
    StaticString<6> str;
    str.assign("abcdef", 6);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // Character sequence length exceeds the maximum string capacity.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<5> str("old");
    IGNORE_ARRAY_BOUNDS_BEGIN
    EXPECT_THROW_OR_ABORT(str.assign("abcdef", 6), std::length_error);
    IGNORE_ARRAY_BOUNDS_END
    EXPECT_STATIC_STR_EQ(str, "old");
  }

  // ### Assign a null-terminated sequence of characters

  {
    StaticString<10> str;
    str.assign("abcdef");
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // Validate the behavior with null terminators in the range.
  {
    StaticString<10> str;
    str.assign("abc\0def");
    EXPECT_STATIC_STR_EQ(str, "abc");
  }

  // Character sequence length matches the maximum string capacity.
  {
    StaticString<6> str;
    str.assign("abcdef");
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // Character sequence length exceeds the maximum string capacity.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<5> str("old");
    EXPECT_THROW_OR_ABORT(str.assign("abcdef"), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "old");
  }

  // ### Assign from an iterator

  {
    const std::string src("abcdef");
    StaticString<10> str;
    str.assign(src.begin(), src.end());
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    const std::string src("abcdef");
    StaticString<5> str("old");
    EXPECT_THROW_OR_ABORT(str.assign(src.begin(), src.end()),
                          std::length_error);
    EXPECT_STATIC_STR_EQ(str, "old");
  }

  // ### Assign from an initializer list

  {
    StaticString<10> str;
    str.assign({'C', '-', 's', 't', 'y', 'l', 'e'});
    EXPECT_STATIC_STR_EQ(str, "C-style");
  }

  // Length of the initializer list matches the maximum string capacity.
  {
    StaticString<7> str;
    str.assign({'C', '-', 's', 't', 'y', 'l', 'e'});
    EXPECT_STATIC_STR_EQ(str, "C-style");
  }

  // Length of the initializer list exceeds the maximum string capacity.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("old");
    IGNORE_ARRAY_BOUNDS_BEGIN
    EXPECT_THROW_OR_ABORT(str.assign({'C', '-', 's', 't', 'y', 'l', 'e'}),
                          std::length_error);
    IGNORE_ARRAY_BOUNDS_END
    EXPECT_STATIC_STR_EQ(str, "old");
  }

  // ### Assign from convertible to a string view

  {
    StaticString<10> str;
    str.assign(std::string("abcdef"));
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // Length of the source matches the maximum capacity.
  {
    StaticString<12> str;
    str.assign(std::string("Hello, World"));
    EXPECT_STATIC_STR_EQ(str, "Hello, World");
  }

  // Length of the source exceeds the maximum capacity.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<11> str("old");
    EXPECT_THROW_OR_ABORT(str.assign(std::string("Hello, World")),
                          std::length_error);
    EXPECT_STATIC_STR_EQ(str, "old");
  }

  // ### Assign a substring of a view-like object

  // Copy npos count.
  {
    const std::string src("abcdef");
    StaticString<10> str;
    str.assign(src, 1);
    EXPECT_STATIC_STR_EQ(str, "bcdef");
  }

  // Copy fixed small count.
  {
    const std::string src("abcdef");
    StaticString<10> str;
    str.assign(src, 1, 2);
    EXPECT_STATIC_STR_EQ(str, "bc");
  }

  // Assign substring with source position past the source size.
  {
    const std::string src(3, 'w');
    StaticString<10> str("old");
    EXPECT_THROW_OR_ABORT(str.assign(src, 10), std::out_of_range);
    EXPECT_STATIC_STR_EQ(str, "old");
  }

  // Length of the source matches the maximum capacity.
  {
    StaticString<4> str;
    str.assign(std::string("Hello, World", 1, 4));
    EXPECT_STATIC_STR_EQ(str, "ello");
  }

  // Length of the source exceeds the maximum capacity.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<3> str("old");
    EXPECT_THROW_OR_ABORT(str.assign(std::string("Hello, World", 1, 4)),
                          std::length_error);
    EXPECT_STATIC_STR_EQ(str, "old");
  }
}

// Element access
// ==============

TEST(tl_static_string, at) {
  // Non-const string.
  {
    StaticString<10> str("abcdef");
    EXPECT_EQ(str.at(1), 'b');
  }

  // Const string.
  {
    const StaticString<10> str("abcdef");
    EXPECT_EQ(str.at(1), 'b');
  }

  // Out of bounds.
  {
    const StaticString<10> str("abcdef");
    EXPECT_THROW_OR_ABORT(str.at(6), std::out_of_range);
  }
}

TEST(tl_static_string, operator_at) {
  // Non-const string.
  {
    StaticString<10> str("abcdef");
    EXPECT_EQ(str[1], 'b');
    EXPECT_EQ(str[6], '\0');
  }

  // Const string.
  {
    const StaticString<10> str("abcdef");
    EXPECT_EQ(str[1], 'b');
    EXPECT_EQ(str[6], '\0');
  }

  // Modification.
  {
    StaticString<10> str("abcdef");
    str[1] = 'x';
    EXPECT_STATIC_STR_EQ(str, "axcdef");
  }
}

TEST(tl_static_string, front) {
  // Non-const string.
  {
    StaticString<10> str("abcdef");
    EXPECT_EQ(str.front(), 'a');
  }

  // Const string.
  {
    const StaticString<10> str("abcdef");
    EXPECT_EQ(str.front(), 'a');
  }

  // Modification.
  {
    StaticString<10> str("abcdef");
    str.front() = 'x';
    EXPECT_STATIC_STR_EQ(str, "xbcdef");
  }
}

TEST(tl_static_string, back) {
  // Non-const string.
  {
    StaticString<10> str("abcdef");
    EXPECT_EQ(str.back(), 'f');
  }

  // Const string.
  {
    const StaticString<10> str("abcdef");
    EXPECT_EQ(str.back(), 'f');
  }

  // Modification.
  {
    StaticString<10> str("abcdef");
    str.back() = 'x';
    EXPECT_STATIC_STR_EQ(str, "abcdex");
  }
}

TEST(tl_static_string, basic_string_view) {
  {
    const StaticString<10> str("abcdef");
    const std::string_view view = (std::string_view)str;
    EXPECT_EQ(view, std::string_view("abcdef"));
  }
}

// Iterators
// =========

TEST(tl_static_string, Iterator) {
  // ### Non-const iterator

  {
    StaticString<10> str("abcdef");
    std::string s;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (StaticString<10>::iterator it = str.begin(); it != str.end(); ++it) {
      s += *it;
    }
    EXPECT_EQ(s, "abcdef");
  }

  {
    StaticString<10> str("abcdef");
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (StaticString<10>::iterator it = str.begin(); it != str.end(); ++it) {
      *it = 'w';
    }
    EXPECT_STATIC_STR_EQ(str, "wwwwww");
  }

  // ### Const iterator

  {
    const StaticString<10> str("abcdef");
    std::string s;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (StaticString<10>::const_iterator it = str.begin(); it != str.end();
         ++it) {
      s += *it;
    }
    EXPECT_EQ(s, "abcdef");
  }

  {
    const StaticString<10> str("abcdef");
    std::string s;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (StaticString<10>::const_iterator it = str.cbegin(); it != str.cend();
         ++it) {
      s += *it;
    }
    EXPECT_EQ(s, "abcdef");
  }
}

TEST(tl_static_string, ReverseIterator) {
  // ### Non-const iterator

  {
    StaticString<10> str("abcdef");
    std::string s;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (StaticString<10>::reverse_iterator it = str.rbegin(); it != str.rend();
         ++it) {
      s += *it;
    }
    EXPECT_EQ(s, "fedcba");
  }

  {
    StaticString<10> str("abcdef");
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (StaticString<10>::reverse_iterator it = str.rbegin(); it != str.rend();
         ++it) {
      *it = 'w';
    }
    EXPECT_STATIC_STR_EQ(str, "wwwwww");
  }

  // ### Const iterator

  {
    const StaticString<10> str("abcdef");
    std::string s;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (StaticString<10>::const_reverse_iterator it = str.rbegin();
         it != str.rend();
         ++it) {
      s += *it;
    }
    EXPECT_EQ(s, "fedcba");
  }

  {
    const StaticString<10> str("abcdef");
    std::string s;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (StaticString<10>::const_reverse_iterator it = str.crbegin();
         it != str.crend();
         ++it) {
      s += *it;
    }
    EXPECT_EQ(s, "fedcba");
  }
}

// Capacity
// ========

TEST(tl_static_string, empty) {
  EXPECT_TRUE(StaticString<10>().empty());
  EXPECT_FALSE(StaticString<10>("x").empty());
}

TEST(tl_static_string, size) {
  EXPECT_EQ(StaticString<10>().size(), 0);
  EXPECT_EQ(StaticString<10>("x").size(), 1);
  EXPECT_EQ(StaticString<10>("abc").size(), 3);
}

TEST(tl_static_string, length) {
  EXPECT_EQ(StaticString<10>().length(), 0);
  EXPECT_EQ(StaticString<10>("x").length(), 1);
  EXPECT_EQ(StaticString<10>("abc").length(), 3);
}

TEST(tl_static_string, max_size) {
  if (!USE_STD_STRING_REFERENCE) {
    EXPECT_EQ(StaticString<10>().max_size(), 10);
    EXPECT_EQ(StaticString<32>().max_size(), 32);
  }
}

TEST(tl_static_string, reserve) {
  StaticString<10>().reserve(1);
  StaticString<10>().reserve(10);

  if (!USE_STD_STRING_REFERENCE) {
    EXPECT_THROW_OR_ABORT(StaticString<10>().reserve(11), std::length_error);
  }
}

TEST(tl_static_string, capacity) {
  if (!USE_STD_STRING_REFERENCE) {
    EXPECT_EQ(StaticString<10>().capacity(), 10);
    EXPECT_EQ(StaticString<32>().capacity(), 32);
  }
}

// Operations
// ==========

TEST(tl_static_string, clear) {
  {
    StaticString<10> str("abcdef");
    str.clear();
    EXPECT_STATIC_STR_EQ(str, "");
  }
}

TEST(tl_static_string, insert) {
  // ### Inserts count copies of character ch at the position index

  {
    StaticString<9> str("abcdef");
    str.insert(2, 3, 'W');
    EXPECT_STATIC_STR_EQ(str, "abWWWcdef");
  }

  // Destination index is above the string limit.
  {
    StaticString<8> str("abcdef");
    EXPECT_THROW_OR_ABORT(str.insert(10, 3, 'x'), std::out_of_range);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // String will exceed capacity after operation.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<8> str("abcdef");
    EXPECT_THROW_OR_ABORT(str.insert(2, 3, 'x'), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // ### Insert a null-terminated character string

  {
    StaticString<9> str("abcdef");
    str.insert(2, "xyz");
    EXPECT_STATIC_STR_EQ(str, "abxyzcdef");
  }

  // Destination index is above the string limit.
  {
    StaticString<8> str("abcdef");
    EXPECT_THROW_OR_ABORT(str.insert(10, "xyz"), std::out_of_range);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // String will exceed capacity after operation.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<8> str("abcdef");
    EXPECT_THROW_OR_ABORT(str.insert(2, "xyz"), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // ### Insert characters range.

  {
    StaticString<9> str("abcdef");
    str.insert(2, "xyz", 3);
    EXPECT_STATIC_STR_EQ(str, "abxyzcdef");
  }

  // Destination index is above the string limit.
  {
    StaticString<8> str("abcdef");
    EXPECT_THROW_OR_ABORT(str.insert(10, "xyz", 3), std::out_of_range);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // Null character in the range.
  {
    StaticString<9> str("abcdef");
    str.insert(2, "x\0z", 3);
    EXPECT_STATIC_STR_EQ(str, std::string_view("abx\0zcdef", 9));
  }

  // String will exceed capacity after operation.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<8> str("abcdef");
    EXPECT_THROW_OR_ABORT(str.insert(2, "xyz", 3), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // ### Insert string at a given position

  {
    StaticString<9> str("abcdef");
    str.insert(2, StaticString<9>("xyz"));
    EXPECT_STATIC_STR_EQ(str, "abxyzcdef");
  }

  // Destination index is above the string limit.
  {
    StaticString<9> str("abcdef");
    EXPECT_THROW_OR_ABORT(str.insert(10, StaticString<9>("xyz")),
                          std::out_of_range);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // String will exceed capacity after operation.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<8> str("abcdef");
    EXPECT_THROW_OR_ABORT(str.insert(2, StaticString<8>("xyz")),
                          std::length_error);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // ### Insert a substring

  // Fixed substring length.
  {
    StaticString<9> str("abcdef");
    str.insert(2, StaticString<9>("qwerty"), 1, 3);
    EXPECT_STATIC_STR_EQ(str, "abwercdef");
  }

  // npos substring length.
  {
    StaticString<9> str("abcdef");
    str.insert(2, StaticString<9>("qwerty"), 3);
    EXPECT_STATIC_STR_EQ(str, "abrtycdef");
  }

  // Destination index is above the string limit.
  {
    StaticString<9> str("abcdef");
    EXPECT_THROW_OR_ABORT(str.insert(10, StaticString<9>("qwerty"), 3),
                          std::out_of_range);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // Insertion string start index is above its length.
  {
    StaticString<9> str("abcdef");
    EXPECT_THROW_OR_ABORT(str.insert(2, StaticString<9>("qwerty"), 10),
                          std::out_of_range);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // ## Insert character before the given iterator

  {
    StaticString<7> str("abcdef");
    EXPECT_EQ(str.insert(str.begin() + 2, 'x'), str.begin() + 2);
    EXPECT_STATIC_STR_EQ(str, "abxcdef");
  }

  // Insert prior to begin().
  {
    StaticString<7> str("abcdef");
    EXPECT_EQ(str.insert(str.begin(), 'x'), str.begin());
    EXPECT_STATIC_STR_EQ(str, "xabcdef");
  }

  // Insert prior to end().
  {
    StaticString<7> str("abcdef");
    EXPECT_EQ(str.insert(str.end(), 'x'), str.begin() + 6);
    EXPECT_STATIC_STR_EQ(str, "abcdefx");
  }

  // String will exceed capacity after operation.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("abcdef");
    EXPECT_THROW_OR_ABORT(str.insert(str.begin() + 2, 'x'), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // ## Insert count copies of a character before the given iterator

  {
    StaticString<9> str("abcdef");
    EXPECT_EQ(str.insert(str.begin() + 2, 3, 'W'), str.begin() + 2);
    EXPECT_STATIC_STR_EQ(str, "abWWWcdef");
  }

  // Insert prior to begin().
  {
    StaticString<9> str("abcdef");
    EXPECT_EQ(str.insert(str.begin(), 3, 'W'), str.begin());
    EXPECT_STATIC_STR_EQ(str, "WWWabcdef");
  }

  // Insert prior to end().
  {
    StaticString<9> str("abcdef");
    EXPECT_EQ(str.insert(str.end(), 3, 'W'), str.begin() + 6);
    EXPECT_STATIC_STR_EQ(str, "abcdefWWW");
  }

  // String will exceed capacity after operation.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<8> str("abcdef");
    EXPECT_THROW_OR_ABORT(str.insert(str.begin() + 2, 3, 'w'),
                          std::length_error);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // ### Insert characters from the range defined by iterators

  {
    std::string sub("abcxyz!");
    StaticString<9> str("abcdef");
    EXPECT_EQ(str.insert(str.begin() + 2, sub.begin() + 3, sub.begin() + 6),
              str.begin() + 2);
    EXPECT_STATIC_STR_EQ(str, "abxyzcdef");
  }

  // Insert prior to begin().
  {
    std::string sub("abcxyz!");
    StaticString<9> str("abcdef");
    EXPECT_EQ(str.insert(str.begin(), sub.begin() + 3, sub.begin() + 6),
              str.begin());
    EXPECT_STATIC_STR_EQ(str, "xyzabcdef");
  }

  // Insert prior to end().
  {
    std::string sub("abcxyz!");
    StaticString<9> str("abcdef");
    EXPECT_EQ(str.insert(str.end(), sub.begin() + 3, sub.begin() + 6),
              str.begin() + 6);
    EXPECT_STATIC_STR_EQ(str, "abcdefxyz");
  }

  // String will exceed capacity after operation.
  if (!USE_STD_STRING_REFERENCE) {
    std::string sub("abcxyz!");
    StaticString<8> str("abcdef");
    EXPECT_THROW_OR_ABORT(
        str.insert(str.begin() + 2, sub.begin() + 3, sub.begin() + 6),
        std::length_error);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // ### Insert elements from an initializer list

  {
    StaticString<9> str("abcdef");
    EXPECT_EQ(str.insert(str.begin() + 2, {'x', 'y', 'z'}), str.begin() + 2);
    EXPECT_STATIC_STR_EQ(str, "abxyzcdef");
  }

  // Insert prior to begin().
  {
    StaticString<9> str("abcdef");
    EXPECT_EQ(str.insert(str.begin(), {'x', 'y', 'z'}), str.begin());
    EXPECT_STATIC_STR_EQ(str, "xyzabcdef");
  }

  // Insert prior to end().
  {
    StaticString<9> str("abcdef");
    EXPECT_EQ(str.insert(str.end(), {'x', 'y', 'z'}), str.begin() + 6);
    EXPECT_STATIC_STR_EQ(str, "abcdefxyz");
  }

  // ### Insert string view at a given position

  {
    StaticString<9> str("abcdef");
    str.insert(2, std::string("xyz"));
    EXPECT_STATIC_STR_EQ(str, "abxyzcdef");
  }

  // Destination index is above the string limit.
  {
    StaticString<9> str("abcdef");
    EXPECT_THROW_OR_ABORT(str.insert(10, std::string("xyz")),
                          std::out_of_range);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // String will exceed capacity after operation.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<8> str("abcdef");
    EXPECT_THROW_OR_ABORT(str.insert(2, std::string("xyz")), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // ### Insert a substring

  // Fixed substring length.
  {
    StaticString<9> str("abcdef");
    str.insert(2, std::string("qwerty"), 1, 3);
    EXPECT_STATIC_STR_EQ(str, "abwercdef");
  }

  // npos substring length.
  {
    StaticString<9> str("abcdef");
    str.insert(2, std::string("qwerty"), 3);
    EXPECT_STATIC_STR_EQ(str, "abrtycdef");
  }

  // Destination index is above the string limit.
  {
    StaticString<9> str("abcdef");
    EXPECT_THROW_OR_ABORT(str.insert(10, std::string("qwerty"), 3),
                          std::out_of_range);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }

  // Insertion string start index is above its length.
  {
    StaticString<9> str("abcdef");
    EXPECT_THROW_OR_ABORT(str.insert(2, std::string("qwerty"), 10),
                          std::out_of_range);
    EXPECT_STATIC_STR_EQ(str, "abcdef");
  }
}

TEST(tl_static_string, erase) {
  // ### Erase number of characters starting at a given index.

  // Erase entire string.
  {
    StaticString<9> str("abcdef");
    str.erase();
    EXPECT_STATIC_STR_EQ(str, "");
  }

  // Erase string starting at index.
  {
    StaticString<9> str("abcdef");
    str.erase(2);
    EXPECT_STATIC_STR_EQ(str, "ab");
  }

  // Erase a sunbstring at index of a given length.
  {
    StaticString<9> str("abcdef");
    str.erase(2, 3);
    EXPECT_STATIC_STR_EQ(str, "abf");
  }

  // ### Remove the character at position.

  {
    StaticString<9> str("abcdef");
    EXPECT_EQ(str.erase(str.begin()), str.begin());
    EXPECT_STATIC_STR_EQ(str, "bcdef");
  }

  {
    StaticString<9> str("abcdef");
    EXPECT_EQ(str.erase(str.begin() + 2), str.begin() + 2);
    EXPECT_STATIC_STR_EQ(str, "abdef");
  }

  // ### Erase range denoted by iterators.

  // Erase entire string.
  {
    StaticString<9> str("abcdef");
    EXPECT_EQ(str.erase(str.begin(), str.end()), str.begin());
    EXPECT_STATIC_STR_EQ(str, "");
  }

  // Erase string starting at a given position.
  {
    StaticString<9> str("abcdef");
    EXPECT_EQ(str.erase(str.begin() + 2, str.end()), str.begin() + 2);
    EXPECT_STATIC_STR_EQ(str, "ab");
  }

  // Erase substring.
  {
    StaticString<9> str("abcdef");
    EXPECT_EQ(str.erase(str.begin() + 2, str.begin() + 5), str.begin() + 2);
    EXPECT_STATIC_STR_EQ(str, "abf");
  }
}

TEST(tl_static_string, push_back) {
  {
    StaticString<9> str;

    str.push_back('f');
    EXPECT_STATIC_STR_EQ(str, "f");

    str.push_back('p');
    EXPECT_STATIC_STR_EQ(str, "fp");
  }

  // Validate overflow behavior.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<3> str("foo");
    EXPECT_THROW_OR_ABORT(str.push_back('a'), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "foo");
  }
}

TEST(tl_static_string, pop_back) {
  {
    StaticString<9> str("foo");

    str.pop_back();
    EXPECT_STATIC_STR_EQ(str, "fo");

    str.pop_back();
    EXPECT_STATIC_STR_EQ(str, "f");

    str.pop_back();
    EXPECT_STATIC_STR_EQ(str, "");
  }

  // Validate overflow behavior.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<3> str("foo");
    EXPECT_THROW_OR_ABORT(str.push_back('a'), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "foo");
  }
}

TEST(tl_static_string, append) {
  // ### Append count copies of a character

  {
    StaticString<7> str("foo");
    str.append(4, 'W');
    EXPECT_STATIC_STR_EQ(str, "fooWWWW");
  }

  // Validate overflow behavior.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("foo");
    EXPECT_THROW_OR_ABORT(str.append(4, 'W'), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "foo");
  }

  // ### Append string

  {
    StaticString<7> str("foo");
    str.append(StaticString<7>("abcd"));
    EXPECT_STATIC_STR_EQ(str, "fooabcd");
  }

  // Validate overflow behavior.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("foo");
    EXPECT_THROW_OR_ABORT(str.append(StaticString<6>("abcd")),
                          std::length_error);
    EXPECT_STATIC_STR_EQ(str, "foo");
  }

  // ### Append a substring

  // Explicit position and count.
  {
    StaticString<8> str("foo");
    str.append(StaticString<8>("abcdefgh"), 2, 4);
    EXPECT_STATIC_STR_EQ(str, "foocdef");
  }

  // Implicit count.
  {
    StaticString<7> str("foo");
    str.append(StaticString<7>("abcdef"), 2);
    EXPECT_STATIC_STR_EQ(str, "foocdef");
  }

  // Substring pos is beyond possible range.
  {
    StaticString<7> str("foo");
    EXPECT_THROW_OR_ABORT(str.append(StaticString<7>("abcdef"), 7),
                          std::out_of_range);
    EXPECT_STATIC_STR_EQ(str, "foo");
  }

  // Validate overflow behavior.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<7> str("foo");
    EXPECT_THROW_OR_ABORT(str.append(StaticString<7>("abcdefgh"), 2, 6),
                          std::length_error);
    EXPECT_STATIC_STR_EQ(str, "foo");
  }

  // ### Append range of characters

  {
    StaticString<7> str("foo");
    str.append("abcd", 4);
    EXPECT_STATIC_STR_EQ(str, "fooabcd");
  }

  // Validate the behavior with null terminators in a range.
  {
    StaticString<7> str("foo");
    str.append("a\0cd", 4);
    EXPECT_STATIC_STR_EQ(str, std::string_view("fooa\0cd", 7));
  }

  // Validate overflow behavior.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("foo");
    EXPECT_THROW_OR_ABORT(str.append("abcd", 4), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "foo");
  }

  // ### Append the null-terminated character string

  {
    StaticString<7> str("foo");
    str.append("abcd");
    EXPECT_STATIC_STR_EQ(str, "fooabcd");
  }

  // Validate overflow behavior.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("foo");
    EXPECT_THROW_OR_ABORT(str.append("abcd"), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "foo");
  }

  // ### Append characters from an iterator range

  {
    const std::string src("abcd");
    StaticString<7> str("foo");
    str.append(src.begin(), src.end());
    EXPECT_STATIC_STR_EQ(str, "fooabcd");
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    const std::string src("abcd");
    StaticString<6> str("foo");
    EXPECT_THROW_OR_ABORT(str.append(src.begin(), src.end()),
                          std::length_error);
    EXPECT_STATIC_STR_EQ(str, "foo");
  }

  // ### Append characters from an initializer list

  {
    StaticString<7> str("foo");
    str.append({'a', 'b', 'c', 'd'});
    EXPECT_STATIC_STR_EQ(str, "fooabcd");
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    const std::initializer_list<char> ilist = {'a', 'b', 'c', 'd'};
    StaticString<6> str("foo");
    EXPECT_THROW_OR_ABORT(str.append(ilist), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "foo");
  }

  // ### Append a string view.

  {
    StaticString<7> str("foo");
    str.append(std::string("abcd"));
    EXPECT_STATIC_STR_EQ(str, "fooabcd");
  }

  // Validate overflow behavior.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("foo");
    EXPECT_THROW_OR_ABORT(str.append(std::string("abcd")), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "foo");
  }

  // ### Append a subview

  // Explicit position and count.
  {
    StaticString<7> str("foo");
    str.append(std::string("abcdefgh"), 2, 4);
    EXPECT_STATIC_STR_EQ(str, "foocdef");
  }

  // Implicit count.
  {
    StaticString<7> str("foo");
    str.append(std::string("abcdefgh"), 6);
    EXPECT_STATIC_STR_EQ(str, "foogh");
  }

  // Substring pos is beyond possible range.
  {
    StaticString<6> str("foo");
    EXPECT_THROW_OR_ABORT(str.append(StaticString<6>("barbaz"), 7),
                          std::out_of_range);
    EXPECT_STATIC_STR_EQ(str, "foo");
  }

  // Validate overflow behavior.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<5> str("foo");
    EXPECT_THROW_OR_ABORT(str.append(StaticString<5>("foobar"), 1, 4),
                          std::length_error);
    EXPECT_STATIC_STR_EQ(str, "foo");
  }
}

TEST(tl_static_string, AppendOperator) {
  // ### Append string

  {
    StaticString<7> str("foo");
    str += StaticString<7>("abcd");
    EXPECT_STATIC_STR_EQ(str, "fooabcd");
  }

  // Validate overflow behavior.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("foo");
    EXPECT_THROW_OR_ABORT(str += StaticString<6>("abcd"), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "foo");
  }

  // ### Append a character

  {
    StaticString<7> str("foo");
    str += 'x';
    EXPECT_STATIC_STR_EQ(str, "foox");
  }

  // Validate overflow behavior.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<3> str("foo");
    EXPECT_THROW_OR_ABORT(str += 'x', std::length_error);
    EXPECT_STATIC_STR_EQ(str, "foo");
  }

  // ### Append the null-terminated character string

  {
    StaticString<7> str("foo");
    str += "abcd";
    EXPECT_STATIC_STR_EQ(str, "fooabcd");
  }

  // Validate overflow behavior.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("foo");
    EXPECT_THROW_OR_ABORT(str += "abcd", std::length_error);
    EXPECT_STATIC_STR_EQ(str, "foo");
  }

  // ### Append characters from an initializer list

  {
    StaticString<7> str("foo");
    str += {'a', 'b', 'c', 'd'};
    EXPECT_STATIC_STR_EQ(str, "fooabcd");
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    const std::initializer_list<char> ilist = {'a', 'b', 'c', 'd'};
    StaticString<6> str("foo");
    EXPECT_THROW_OR_ABORT(str += ilist, std::length_error);
    EXPECT_STATIC_STR_EQ(str, "foo");
  }

  // ### Append a string view.

  {
    StaticString<7> str("foo");
    str += std::string("abcd");
    EXPECT_STATIC_STR_EQ(str, "fooabcd");
  }

  // Validate overflow behavior.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("foo");
    EXPECT_THROW_OR_ABORT(str += std::string("abcd"), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "foo");
  }
}

TEST(tl_static_string, compare) {
  // Compare to a string.
  EXPECT_EQ(StaticString<6>("123").compare(StaticString<6>("123")), 0);
  EXPECT_LT(StaticString<6>("12").compare(StaticString<6>("123")), 0);
  EXPECT_GT(StaticString<6>("123").compare(StaticString<6>("12")), 0);
  EXPECT_LT(StaticString<6>("122").compare(StaticString<6>("123")), 0);
  EXPECT_GT(StaticString<6>("123").compare(StaticString<6>("122")), 0);

  // Compare substring to a string.
  EXPECT_EQ(StaticString<6>("01234").compare(1, 3, StaticString<6>("123")), 0);
  EXPECT_LT(StaticString<6>("01234").compare(0, 3, StaticString<6>("123")), 0);
  EXPECT_GT(StaticString<6>("01234").compare(2, 3, StaticString<6>("123")), 0);
  EXPECT_LT(StaticString<6>("01234").compare(1, 2, StaticString<6>("123")), 0);
  EXPECT_GT(StaticString<6>("01234").compare(1, 4, StaticString<6>("123")), 0);

  // Compare substring to a substring.
  EXPECT_EQ(
      StaticString<6>("01234").compare(1, 3, StaticString<6>("01234"), 1, 3),
      0);
  EXPECT_EQ(
      StaticString<6>("01234").compare(0, 3, StaticString<6>("34012"), 2, 3),
      0);
  EXPECT_LT(
      StaticString<6>("01234").compare(1, 3, StaticString<6>("01234"), 2, 3),
      0);
  EXPECT_GT(
      StaticString<6>("01234").compare(1, 3, StaticString<6>("01234"), 1, 2),
      0);

  // Compare to a null-terminated character string.
  EXPECT_EQ(StaticString<6>("123").compare("123"), 0);
  EXPECT_LT(StaticString<6>("12").compare("123"), 0);
  EXPECT_GT(StaticString<6>("123").compare("12"), 0);
  EXPECT_LT(StaticString<6>("122").compare("123"), 0);
  EXPECT_GT(StaticString<6>("123").compare("122"), 0);

  // Compare substring to a null-terminated character string.
  EXPECT_EQ(StaticString<6>("01234").compare(1, 3, "123"), 0);
  EXPECT_LT(StaticString<6>("01234").compare(0, 3, "123"), 0);
  EXPECT_GT(StaticString<6>("01234").compare(2, 3, "123"), 0);
  EXPECT_LT(StaticString<6>("01234").compare(1, 2, "123"), 0);
  EXPECT_GT(StaticString<6>("01234").compare(1, 4, "123"), 0);

  // Compare substring to a substring of a null-terminated character string.
  EXPECT_EQ(StaticString<6>("01234").compare(1, 3, "1234", 3), 0);
  EXPECT_GT(StaticString<6>("01234").compare(1, 3, "01234", 3), 0);
  EXPECT_LT(StaticString<6>("01234").compare(1, 3, "23456", 3), 0);

  // Compare to a string view.
  EXPECT_EQ(StaticString<6>("123").compare(std::string("123")), 0);
  EXPECT_LT(StaticString<6>("12").compare(std::string("123")), 0);
  EXPECT_GT(StaticString<6>("123").compare(std::string("12")), 0);
  EXPECT_LT(StaticString<6>("122").compare(std::string("123")), 0);
  EXPECT_GT(StaticString<6>("123").compare(std::string("122")), 0);

  // Compare substring to a string view.
  EXPECT_EQ(StaticString<6>("01234").compare(1, 3, std::string("123")), 0);
  EXPECT_LT(StaticString<6>("01234").compare(0, 3, std::string("123")), 0);
  EXPECT_GT(StaticString<6>("01234").compare(2, 3, std::string("123")), 0);
  EXPECT_LT(StaticString<6>("01234").compare(1, 2, std::string("123")), 0);
  EXPECT_GT(StaticString<6>("01234").compare(1, 4, std::string("123")), 0);

  // Compare substring to a substring of a view.
  EXPECT_EQ(StaticString<6>("01234").compare(1, 3, std::string("01234"), 1, 3),
            0);
  EXPECT_EQ(StaticString<6>("01234").compare(0, 3, std::string("34012"), 2, 3),
            0);
  EXPECT_LT(StaticString<6>("01234").compare(1, 3, std::string("01234"), 2, 3),
            0);
  EXPECT_GT(StaticString<6>("01234").compare(1, 3, std::string("01234"), 1, 2),
            0);
}

TEST(tl_static_string, starts_with) {
  // Check for starts with a string.
  EXPECT_FALSE(StaticString<6>("").starts_with(std::string("abc")));
  EXPECT_FALSE(StaticString<6>("ab").starts_with(std::string("abc")));
  EXPECT_TRUE(StaticString<6>("abc").starts_with(std::string("abc")));
  EXPECT_TRUE(StaticString<6>("abcd").starts_with(std::string("abc")));
  EXPECT_FALSE(StaticString<6>("xabcd").starts_with(std::string("abc")));
  EXPECT_TRUE(StaticString<6>("").starts_with(std::string("")));
  EXPECT_TRUE(StaticString<6>("abc").starts_with(std::string("")));

  // Check for starts with a character.
  EXPECT_FALSE(StaticString<6>("").starts_with('x'));
  EXPECT_FALSE(StaticString<6>("").starts_with('\0'));
  EXPECT_FALSE(StaticString<6>("a").starts_with('\0'));
  EXPECT_FALSE(StaticString<6>("abc").starts_with('x'));
  EXPECT_TRUE(StaticString<6>("xabc").starts_with('x'));

  // Check for starts with a null-terminated character string.
  EXPECT_FALSE(StaticString<6>("").starts_with("abc"));
  EXPECT_FALSE(StaticString<6>("ab").starts_with("abc"));
  EXPECT_TRUE(StaticString<6>("abc").starts_with("abc"));
  EXPECT_TRUE(StaticString<6>("abcd").starts_with("abc"));
  EXPECT_TRUE(StaticString<6>("").starts_with(""));
  EXPECT_TRUE(StaticString<6>("abc").starts_with(""));
}

TEST(tl_static_string, ends_with) {
  // Check for ends with a string.
  EXPECT_FALSE(StaticString<6>("").ends_with(std::string("abc")));
  EXPECT_FALSE(StaticString<6>("ab").ends_with(std::string("abc")));
  EXPECT_TRUE(StaticString<6>("abc").ends_with(std::string("abc")));
  EXPECT_FALSE(StaticString<6>("abcd").ends_with(std::string("abc")));
  EXPECT_TRUE(StaticString<6>("").ends_with(std::string("")));
  EXPECT_TRUE(StaticString<6>("abc").ends_with(std::string("")));

  // Check for ends with a character.
  EXPECT_FALSE(StaticString<6>("").ends_with('x'));
  EXPECT_FALSE(StaticString<6>("").ends_with('\0'));
  EXPECT_FALSE(StaticString<6>("a").ends_with('\0'));
  EXPECT_FALSE(StaticString<6>("abc").ends_with('x'));
  EXPECT_TRUE(StaticString<6>("abcx").ends_with('x'));

  // Check for ends with a null-terminated character string.
  EXPECT_FALSE(StaticString<6>("").ends_with("abc"));
  EXPECT_FALSE(StaticString<6>("ab").ends_with("abc"));
  EXPECT_TRUE(StaticString<6>("abc").ends_with("abc"));
  EXPECT_FALSE(StaticString<6>("abcd").ends_with("abc"));
  EXPECT_TRUE(StaticString<6>("").ends_with(""));
  EXPECT_TRUE(StaticString<6>("abc").ends_with(""));
}

#if !USE_STD_STRING_REFERENCE
TEST(tl_static_string, contains) {
  using StaticString = StaticString<24>;

  EXPECT_TRUE(StaticString("abcdef").contains(std::string("bcd")));
  EXPECT_FALSE(StaticString("abcdef").contains(std::string("xyz")));

  EXPECT_TRUE(StaticString("abcdef").contains('c'));
  EXPECT_FALSE(StaticString("abcdef").contains('x'));

  EXPECT_TRUE(StaticString("abcdef").contains("bcd"));
  EXPECT_FALSE(StaticString("abcdef").contains("xyz"));
}
#endif

TEST(tl_static_string, replace) {
  // ### Replace substring defined by pos and count with a stirng.

  {
    EXPECT_STATIC_STR_EQ(
        StaticString<8>("012345").replace(1, 2, StaticString<8>("abcd")),
        "0abcd345");
  }

  // Count is past the actual size.
  {
    EXPECT_STATIC_STR_EQ(
        StaticString<6>("012345").replace(1, 20, StaticString<6>("abcd")),
        "0abcd");
  }

  // Out of bounds position.
  {
    StaticString<6> str("01234");
    EXPECT_THROW_OR_ABORT(str.replace(7, 2, StaticString<6>("abcd")),
                          std::out_of_range);
    EXPECT_STATIC_STR_EQ(str, "01234");
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("01234");
    EXPECT_THROW_OR_ABORT(str.replace(1, 2, StaticString<6>("abcd")),
                          std::length_error);
    EXPECT_STATIC_STR_EQ(str, "01234");
  }

  // ### Replace substring defined by iterators with a stirng.

  {
    StaticString<8> str("012345");
    EXPECT_STATIC_STR_EQ(
        str.replace(str.begin() + 1, str.begin() + 3, StaticString<8>("abcd")),
        "0abcd345");
  }

  // Last is past the actual size.
  {
    StaticString<8> str("012345");
    EXPECT_STATIC_STR_EQ(
        str.replace(str.begin() + 1, str.begin() + 20, StaticString<8>("abcd")),
        "0abcd");
  }

  // Replace up to the end of the string.
  {
    StaticString<8> str("012345");
    EXPECT_STATIC_STR_EQ(
        str.replace(str.begin() + 1, str.end(), StaticString<8>("abcd")),
        "0abcd");
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("01234");
    EXPECT_THROW_OR_ABORT(str.replace(str.begin() + 1, str.begin() + 3, "abcd"),
                          std::length_error);
    EXPECT_STATIC_STR_EQ(str, StaticString<6>("01234"));
  }

  // ### Replace substring with a substring using pos and count.

  {
    EXPECT_STATIC_STR_EQ(StaticString<8>("012345").replace(
                             1, 2, StaticString<8>("abcdefgh"), 2, 4),
                         "0cdef345");
  }

  // Count is past the actual size.
  {
    EXPECT_STATIC_STR_EQ(StaticString<8>("012345").replace(
                             1, 20, StaticString<8>("abcdefgh"), 2, 4),
                         "0cdef");
  }

  // Count2 is past the actual size.
  {
    EXPECT_STATIC_STR_EQ(StaticString<10>("012345").replace(
                             1, 2, StaticString<10>("abcdefgh"), 2, 20),
                         "0cdefgh345");
  }

  // Out of bounds position.
  {
    StaticString<6> str("01234");
    EXPECT_THROW_OR_ABORT(str.replace(7, 2, StaticString<6>("abcd"), 1, 2),
                          std::out_of_range);
    EXPECT_STATIC_STR_EQ(str, "01234");
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("01234");
    EXPECT_THROW_OR_ABORT(str.replace(1, 2, StaticString<6>("abcdef"), 1, 4),
                          std::length_error);
    EXPECT_STATIC_STR_EQ(str, "01234");
  }

  // ### Replace substring with a string using iterator range.

  {
    const std::string src("abcdefgh");
    StaticString<8> str("012345");
    EXPECT_STATIC_STR_EQ(
        str.replace(
            str.begin() + 1, str.begin() + 3, src.begin() + 2, src.begin() + 6),
        "0cdef345");
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    const std::string src("abcdefgh");
    StaticString<6> str("01234");
    EXPECT_THROW_OR_ABORT(
        str.replace(
            str.begin() + 1, str.begin() + 3, src.begin() + 2, src.begin() + 6),
        std::length_error);
    EXPECT_STATIC_STR_EQ(str, "01234");
  }

  // ### Replace substring with a substring of a character sequence.

  {
    EXPECT_STATIC_STR_EQ(StaticString<8>("012345").replace(1, 2, "cdefgh", 4),
                         "0cdef345");
  }

  // Count is past the actual size.
  {
    EXPECT_STATIC_STR_EQ(StaticString<8>("012345").replace(1, 20, "cdefgh", 4),
                         "0cdef");
  }

  // Out of bounds position.
  {
    StaticString<6> str("01234");
    EXPECT_THROW_OR_ABORT(str.replace(7, 2, "bcd", 2), std::out_of_range);
    EXPECT_STATIC_STR_EQ(str, "01234");
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("01234");
    EXPECT_THROW_OR_ABORT(str.replace(1, 2, "bcdef", 4), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "01234");
  }

  // ### Replace substring range with a substring.

  {
    StaticString<8> str("012345");
    EXPECT_STATIC_STR_EQ(
        str.replace(str.begin() + 1, str.begin() + 3, "cdefgh", 4), "0cdef345");
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("01234");
    EXPECT_THROW_OR_ABORT(
        str.replace(str.begin() + 1, str.begin() + 3, "cdefgh", 4),
        std::length_error);
    EXPECT_STATIC_STR_EQ(str, "01234");
  }

  // ### Replace substring range with a null-terminated substring.

  {
    StaticString<8> str("012345");
    EXPECT_STATIC_STR_EQ(str.replace(str.begin() + 1, str.begin() + 3, "cdef"),
                         "0cdef345");
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("01234");
    EXPECT_THROW_OR_ABORT(str.replace(str.begin() + 1, str.begin() + 3, "cdef"),
                          std::length_error);
    EXPECT_STATIC_STR_EQ(str, "01234");
  }

  // ### Replace substring with copies of character.

  {
    StaticString<8> str("012345");
    EXPECT_STATIC_STR_EQ(str.replace(1, 2, 4, 'w'), "0wwww345");
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("01234");
    EXPECT_THROW_OR_ABORT(str.replace(1, 2, 4, 'x'), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "01234");
  }

  // ### Replace substring range with copies of character.

  {
    StaticString<8> str("012345");
    EXPECT_STATIC_STR_EQ(str.replace(str.begin() + 1, str.begin() + 3, 4, 'w'),
                         "0wwww345");
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("01234");
    EXPECT_THROW_OR_ABORT(str.replace(str.begin() + 1, str.begin() + 3, 4, 'w'),
                          std::length_error);
    EXPECT_STATIC_STR_EQ(str, "01234");
  }

  // ### Replace substring range with an initializer list.

  {
    StaticString<8> str("012345");
    EXPECT_STATIC_STR_EQ(
        str.replace(str.begin() + 1, str.begin() + 3, {'c', 'd', 'e', 'f'}),
        "0cdef345");
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    const std::initializer_list<char> ilist = {'c', 'd', 'e', 'f'};
    StaticString<6> str("01234");
    EXPECT_THROW_OR_ABORT(str.replace(str.begin() + 1, str.begin() + 3, ilist),
                          std::length_error);
    EXPECT_STATIC_STR_EQ(str, "01234");
  }

  // ### Replace substring with a view.

  {
    EXPECT_STATIC_STR_EQ(
        StaticString<8>("012345").replace(1, 2, std::string("abcd")),
        "0abcd345");
  }

  // Count is past the actual size.
  {
    EXPECT_STATIC_STR_EQ(
        StaticString<6>("012345").replace(1, 20, std::string("abcd")), "0abcd");
  }

  // Out of bounds position.
  {
    StaticString<6> str("01234");
    EXPECT_THROW_OR_ABORT(str.replace(7, 2, std::string("abcd")),
                          std::out_of_range);
    EXPECT_STATIC_STR_EQ(str, "01234");
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("01234");
    EXPECT_THROW_OR_ABORT(str.replace(1, 2, std::string("abcd")),
                          std::length_error);
    EXPECT_STATIC_STR_EQ(str, "01234");
  }

  // ### Replace substring defined by iterators with a view.

  {
    StaticString<8> str("012345");
    EXPECT_STATIC_STR_EQ(
        str.replace(str.begin() + 1, str.begin() + 3, std::string("abcd")),
        "0abcd345");
  }

  // Last is past the actual size.
  {
    StaticString<8> str("012345");
    EXPECT_STATIC_STR_EQ(
        str.replace(str.begin() + 1, str.begin() + 20, std::string("abcd")),
        "0abcd");
  }

  // Replace up to the end of the string.
  {
    StaticString<8> str("012345");
    EXPECT_STATIC_STR_EQ(
        str.replace(str.begin() + 1, str.end(), std::string("abcd")), "0abcd");
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("01234");
    EXPECT_THROW_OR_ABORT(str.replace(str.begin() + 1, str.begin() + 3, "abcd"),
                          std::length_error);
    EXPECT_STATIC_STR_EQ(str, std::string("01234"));
  }

  // ### Replace substring with a subview.

  {
    EXPECT_STATIC_STR_EQ(
        StaticString<8>("012345").replace(1, 2, std::string("abcdefgh"), 2, 4),
        "0cdef345");
  }

  // Count is past the actual size.
  {
    EXPECT_STATIC_STR_EQ(
        StaticString<8>("012345").replace(1, 20, std::string("abcdefgh"), 2, 4),
        "0cdef");
  }

  // Count2 is past the actual size.
  {
    EXPECT_STATIC_STR_EQ(StaticString<10>("012345").replace(
                             1, 2, std::string("abcdefgh"), 2, 20),
                         "0cdefgh345");
  }

  // Out of bounds position.
  {
    StaticString<6> str("01234");
    EXPECT_THROW_OR_ABORT(str.replace(7, 2, std::string("abcd"), 1, 2),
                          std::out_of_range);
    EXPECT_STATIC_STR_EQ(str, "01234");
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("01234");
    EXPECT_THROW_OR_ABORT(str.replace(1, 2, std::string("abcdef"), 1, 4),
                          std::length_error);
    EXPECT_STATIC_STR_EQ(str, "01234");
  }
}

TEST(tl_static_string, substr) {
  EXPECT_STATIC_STR_EQ(StaticString<6>("012345").substr(), "012345");
  EXPECT_STATIC_STR_EQ(StaticString<6>("012345").substr(1), "12345");
  EXPECT_STATIC_STR_EQ(StaticString<6>("012345").substr(1, 2), "12");
  EXPECT_THROW_OR_ABORT(StaticString<6>("012345").substr(10),
                        std::out_of_range);
}

TEST(tl_static_string, copy) {
  {
    std::string str("abcdefghij");
    EXPECT_EQ(StaticString<6>("0123").copy(&str[3], 2, 1), 2);
    EXPECT_EQ(str, "abc12fghij");
  }

  {
    std::string str("abcdefghij");
    EXPECT_EQ(StaticString<6>("0123").copy(&str[3], 20, 1), 3);
    EXPECT_EQ(str, "abc123ghij");
  }

  {
    std::string str("abcdefgh");
    EXPECT_THROW_OR_ABORT(StaticString<6>("012").copy(&str[1], 2, 10),
                          std::out_of_range);
  }
}

TEST(tl_static_string, resize) {
  // Expand with a null-terminator.
  {
    StaticString<6> str("0123");
    str.resize(6);
    EXPECT_STATIC_STR_EQ(str, std::string_view("0123\0\0", 6));
  }

  // Expand with an explicit character.
  {
    StaticString<6> str("0123");
    str.resize(6, 'x');
    EXPECT_STATIC_STR_EQ(str, std::string_view("0123xx", 6));
  }

  // Shrink down.
  {
    StaticString<6> str("0123");
    str.resize(2);
    EXPECT_STATIC_STR_EQ(str, "01");
  }

  // Completely erase.
  {
    StaticString<6> str("0123");
    str.resize(0);
    EXPECT_STATIC_STR_EQ(str, "");
  }

  // Ensure behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("012345");
    EXPECT_THROW_OR_ABORT(str.resize(7), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "012345");
  }
}

TEST(tl_static_string, swap) {
  {
    StaticString<6> a("012345");
    StaticString<6> b("abcd");

    a.swap(b);

    EXPECT_STATIC_STR_EQ(a, "abcd");
    EXPECT_STATIC_STR_EQ(b, "012345");
  }
}

#if !USE_STD_STRING_REFERENCE
TEST(tl_static_string, resize_and_overwrite) {
  // Validate behavior on overflow.
  if (!USE_STD_STRING_REFERENCE) {
    StaticString<6> str("012345");
    auto op = [](char* /*buf*/, size_t buf_size) { return buf_size; };
    EXPECT_THROW_OR_ABORT(str.resize_and_overwrite(16, op), std::length_error);
    EXPECT_STATIC_STR_EQ(str, "012345");
  }

  // ### Tests from C++ reference documentation page.

  const auto fruits = std::to_array<std::string_view>(
      {"apple", "banana", "coconut", "date", "elderberry"});

  // A simple case, append only fruits[0]. The string size will be increased.
  {
    StaticString<32> s{"Food: "};
    s.resize_and_overwrite(
        16, [sz = s.size(), &fruits](char* buf, std::size_t buf_size) {
          const auto to_copy = std::min(buf_size - sz, fruits[0].size());
          std::memcpy(buf + sz, fruits[0].data(), to_copy);
          return sz + to_copy;
        });
    EXPECT_STATIC_STR_EQ(s, "Food: apple");
  }

  // The size shrinking case. Note, that the user's lambda is always invoked.
  {
    StaticString<32> s{"Food: "};
    s.resize_and_overwrite(10, [](char* buf, int n) {
      return std::find(buf, buf + n, ':') - buf;
    });
    EXPECT_STATIC_STR_EQ(s, "Food");
  }

  // Copy data until the buffer is full.
  {
    StaticString<32> food{"Food:"};
    food.resize_and_overwrite(
        27,
        [food_size = food.size(), &fruits](
            char* p, std::size_t n) noexcept -> std::size_t {
          char* first = p + food_size;

          for (char* const end = p + n; const std::string_view fruit : fruits) {
            char* last = first + fruit.size() + 1;
            if (last > end) {
              break;
            }
            *first++ = ' ';
            fruit.copy(first, fruit.size());
            first = last;
          }

          return first - p;
        });

    EXPECT_STATIC_STR_EQ(food, "Food: apple banana coconut");
  }
}
#endif

TEST(tl_static_string, find) {
  using StaticString = StaticString<24>;

  EXPECT_EQ(StaticString("This is a string").find(StaticString("is")), 2);
  EXPECT_EQ(StaticString("This is a string").find(StaticString("is"), 4), 5);
  EXPECT_EQ(StaticString("This is a string").find(StaticString("foo")),
            StaticString::npos);

  EXPECT_EQ(StaticString("This is a string").find("isx", 0, 2), 2);
  EXPECT_EQ(StaticString("This is a string").find("isx", 4, 2), 5);
  EXPECT_EQ(StaticString("Thi\0s i\0s a string", 18).find("i\0sx", 0, 3), 2);
  EXPECT_EQ(StaticString("This is a string").find("foo", 4, 2),
            StaticString::npos);

  EXPECT_EQ(StaticString("This is a string").find("is", 0, 2), 2);
  EXPECT_EQ(StaticString("This is a string").find("is", 4, 2), 5);
  EXPECT_EQ(StaticString("This is a string").find("foo", 4, 2),
            StaticString::npos);

  EXPECT_EQ(StaticString("This is a string").find('i'), 2);
  EXPECT_EQ(StaticString("This is a string").find('i', 3), 5);
  EXPECT_EQ(StaticString("This is a string").find('x'), StaticString::npos);

  EXPECT_EQ(StaticString("This is a string").find(std::string("is")), 2);
  EXPECT_EQ(StaticString("This is a string").find(std::string("is"), 4), 5);
  EXPECT_EQ(StaticString("This is a string").find(std::string("foo")),
            StaticString::npos);
}

TEST(tl_static_string, rfind) {
  using StaticString = StaticString<24>;

  EXPECT_EQ(StaticString("This is a string").rfind(StaticString("is")), 5);
  EXPECT_EQ(StaticString("This is a string").rfind(StaticString("is"), 4), 2);
  EXPECT_EQ(StaticString("This is a string").rfind(StaticString("foo")),
            StaticString::npos);

  EXPECT_EQ(StaticString("This is a string").rfind("isx", 10, 2), 5);
  EXPECT_EQ(StaticString("This is a string").rfind("isx", 4, 2), 2);
  EXPECT_EQ(StaticString("Thi\0s i\0s a string", 18).rfind("i\0sx", 10, 3), 6);
  EXPECT_EQ(StaticString("This is a string").rfind("foo", 4, 2),
            StaticString::npos);

  EXPECT_EQ(StaticString("This is a string").rfind("is", 10, 2), 5);
  EXPECT_EQ(StaticString("This is a string").rfind("is", 4, 2), 2);
  EXPECT_EQ(StaticString("This is a string").rfind("foo", 6, 2),
            StaticString::npos);

  EXPECT_EQ(StaticString("This is a string").rfind('i'), 13);
  EXPECT_EQ(StaticString("This is a string").rfind('i', 8), 5);
  EXPECT_EQ(StaticString("This is a string").rfind('x'), StaticString::npos);

  EXPECT_EQ(StaticString("This is a string").rfind(std::string("is")), 5);
  EXPECT_EQ(StaticString("This is a string").rfind(std::string("is"), 4), 2);
  EXPECT_EQ(StaticString("This is a string").rfind(std::string("foo")),
            StaticString::npos);
}

TEST(tl_static_string, find_first_of) {
  using StaticString = StaticString<24>;

  // Test is based on C++ reference page.

  const char* buf = "xyzabc";

  EXPECT_EQ(StaticString("alignas").find_first_of(StaticString("klmn")), 1);
  EXPECT_EQ(StaticString("alignas").find_first_of(StaticString("klmn"), 2), 4);
  EXPECT_EQ(StaticString("alignas").find_first_of(StaticString("xyzw")),
            StaticString::npos);

  EXPECT_EQ(StaticString("consteval").find_first_of(buf, 0, 6), 0);
  EXPECT_EQ(StaticString("consteval").find_first_of(buf, 1, 6), 7);
  EXPECT_EQ(StaticString("consteval").find_first_of(buf, 0, 3),
            StaticString::npos);

  EXPECT_EQ(StaticString("decltype").find_first_of(buf), 2);
  EXPECT_EQ(StaticString("declvar").find_first_of(buf, 3), 5);
  EXPECT_EQ(StaticString("hello").find_first_of(buf), StaticString::npos);

  EXPECT_EQ(StaticString("co_await").find_first_of('a'), 3);
  EXPECT_EQ(StaticString("co_await").find_first_of('a', 4), 5);
  EXPECT_EQ(StaticString("co_await").find_first_of('x'), StaticString::npos);

  EXPECT_EQ(StaticString("constinit").find_first_of(std::string("int")), 2);
  EXPECT_EQ(StaticString("constinit").find_first_of(std::string("int"), 3), 4);
  EXPECT_EQ(StaticString("constinit").find_first_of(std::string("xyz")),
            StaticString::npos);
}

TEST(tl_static_string, find_first_not_of) {
  using StaticString = StaticString<24>;

  const char* buf = "xyzabc";

  EXPECT_EQ(StaticString("xyzUxVW").find_first_not_of(StaticString(buf)), 3);
  EXPECT_EQ(StaticString("xyzUxVW").find_first_not_of(StaticString(buf), 4), 5);
  EXPECT_EQ(StaticString("xyzxyz").find_first_not_of(StaticString(buf), 4),
            StaticString::npos);

  EXPECT_EQ(StaticString("xyzcxUW").find_first_not_of(buf, 0, 5), 3);
  EXPECT_EQ(StaticString("xyzcxUW").find_first_not_of(buf, 4, 5), 5);
  EXPECT_EQ(StaticString("xyzxyz").find_first_not_of(buf, 4, 5),
            StaticString::npos);

  EXPECT_EQ(StaticString("xyzUxVW").find_first_not_of(buf), 3);
  EXPECT_EQ(StaticString("xyzUxVW").find_first_not_of(buf, 4), 5);
  EXPECT_EQ(StaticString("xyzxyz").find_first_not_of(buf, 4),
            StaticString::npos);

  EXPECT_EQ(StaticString("xyxzabc").find_first_not_of('x'), 1);
  EXPECT_EQ(StaticString("xyxzabc").find_first_not_of('x', 2), 3);
  EXPECT_EQ(StaticString("www").find_first_not_of('w'), StaticString::npos);

  EXPECT_EQ(StaticString("xyzUxVW").find_first_not_of(std::string(buf)), 3);
  EXPECT_EQ(StaticString("xyzUxVW").find_first_not_of(std::string(buf), 4), 5);
  EXPECT_EQ(StaticString("xyzxyz").find_first_not_of(std::string(buf), 4),
            StaticString::npos);
}

TEST(tl_static_string, find_last_of) {
  using StaticString = StaticString<24>;

  // Test is based on C++ reference page.

  const char* buf = "xyzabc";

  EXPECT_EQ(StaticString("alignas").find_last_of(StaticString("klmn")), 4);
  EXPECT_EQ(StaticString("alignas").find_last_of(StaticString("klmn"), 3), 1);
  EXPECT_EQ(StaticString("alignas").find_last_of(StaticString("xyzw")),
            StaticString::npos);

  EXPECT_EQ(StaticString("consteval").find_last_of(buf, 8, 6), 7);
  EXPECT_EQ(StaticString("consteval").find_last_of(buf, 5, 6), 0);
  EXPECT_EQ(StaticString("consteval").find_last_of(buf, 0, 3),
            StaticString::npos);

  EXPECT_EQ(StaticString("decltype").find_last_of(buf), 5);
  EXPECT_EQ(StaticString("decltype").find_last_of(buf, 4), 2);
  EXPECT_EQ(StaticString("hello").find_last_of(buf), StaticString::npos);

  EXPECT_EQ(StaticString("co_await").find_last_of('a'), 5);
  EXPECT_EQ(StaticString("co_await").find_last_of('a', 4), 3);
  EXPECT_EQ(StaticString("co_await").find_last_of('x'), StaticString::npos);

  EXPECT_EQ(StaticString("constinit").find_last_of(std::string("int")), 8);
  EXPECT_EQ(StaticString("constinit").find_last_of(std::string("int"), 6), 6);
  EXPECT_EQ(StaticString("constinit").find_last_of(std::string("xyz")),
            StaticString::npos);
}

TEST(tl_static_string, find_last_not_of) {
  using StaticString = StaticString<24>;

  const char* buf = "xyzabc";

  EXPECT_EQ(StaticString("xyzUxVWx").find_last_not_of(StaticString(buf)), 6);
  EXPECT_EQ(StaticString("xyzUxVWx").find_last_not_of(StaticString(buf), 4), 3);
  EXPECT_EQ(StaticString("xyzxyz").find_last_not_of(StaticString(buf), 4),
            StaticString::npos);

  EXPECT_EQ(StaticString("xyzcxUcx").find_last_not_of(buf, 7, 5), 6);
  EXPECT_EQ(StaticString("xyzcxUcx").find_last_not_of(buf, 4, 4), 3);
  EXPECT_EQ(StaticString("xyzxyz").find_last_not_of(buf, 4, 5),
            StaticString::npos);

  EXPECT_EQ(StaticString("xyzUxVWc").find_last_not_of(buf), 6);
  EXPECT_EQ(StaticString("xyzUxxVWc").find_last_not_of(buf, 5), 3);
  EXPECT_EQ(StaticString("xyzxyz").find_last_not_of(buf, 4),
            StaticString::npos);

  EXPECT_EQ(StaticString("xyzabxcx").find_last_not_of('x'), 6);
  EXPECT_EQ(StaticString("xyzabxcx").find_last_not_of('x', 5), 4);
  EXPECT_EQ(StaticString("www").find_last_not_of('w'), StaticString::npos);

  EXPECT_EQ(StaticString("xyzUxVWx").find_last_not_of(std::string(buf)), 6);
  EXPECT_EQ(StaticString("xyzUxVWx").find_last_not_of(std::string(buf), 4), 3);
  EXPECT_EQ(StaticString("xyzxyz").find_last_not_of(std::string(buf), 4),
            StaticString::npos);
}

////////////////////////////////////////////////////////////////////////////////
// Non-member functions.

TEST(tl_static_string, operator_add) {
  using StaticString = StaticString<8>;

  const StaticString foo("foo");
  const StaticString bar("bar");

  EXPECT_STATIC_STR_EQ(foo + bar, "foobar");
  EXPECT_STATIC_STR_EQ(foo + "bar", "foobar");
  EXPECT_STATIC_STR_EQ(foo + 'b', "foob");
  EXPECT_STATIC_STR_EQ("bar" + StaticString("foo"), "barfoo");
  EXPECT_STATIC_STR_EQ('b' + StaticString("foo"), "bfoo");

  EXPECT_STATIC_STR_EQ(StaticString("foo") + StaticString("bar"), "foobar");
  EXPECT_STATIC_STR_EQ(StaticString("foo") + bar, "foobar");
  EXPECT_STATIC_STR_EQ(StaticString("foo") + "bar", "foobar");
  EXPECT_STATIC_STR_EQ(StaticString("foo") + 'b', "foob");
  EXPECT_STATIC_STR_EQ(bar + StaticString("foo"), "barfoo");
  EXPECT_STATIC_STR_EQ("bar" + StaticString("foo"), "barfoo");
  EXPECT_STATIC_STR_EQ('b' + StaticString("foo"), "bfoo");
  EXPECT_STATIC_STR_EQ('b' + foo, "bfoo");
}

TEST(tl_static_string, operator_equals) {
  using StaticString = StaticString<24>;

  EXPECT_TRUE(StaticString("foo") == StaticString("foo"));
  EXPECT_FALSE(StaticString("foo") == StaticString("bar"));

  EXPECT_TRUE(StaticString("foo") == "foo");
  EXPECT_FALSE(StaticString("foo") == "bar");
}

TEST(tl_static_string, operator_compare) {
  using StaticString = StaticString<24>;

  EXPECT_TRUE(StaticString("foo") == StaticString("foo"));
  EXPECT_FALSE(StaticString("foo") == StaticString("bar"));

  EXPECT_TRUE(StaticString("foo") == "foo");
  EXPECT_FALSE(StaticString("foo") == "bar");

  EXPECT_TRUE(StaticString("12") < StaticString("123"));
  EXPECT_FALSE(StaticString("12") > StaticString("123"));
  EXPECT_TRUE(StaticString("123") > StaticString("12"));
  EXPECT_FALSE(StaticString("123") < StaticString("12"));
  EXPECT_TRUE(StaticString("122") < StaticString("123"));
  EXPECT_FALSE(StaticString("122") > StaticString("123"));
  EXPECT_TRUE(StaticString("123") > StaticString("122"));
  EXPECT_FALSE(StaticString("123") < StaticString("122"));

  EXPECT_TRUE(StaticString("12") < "123");
  EXPECT_FALSE(StaticString("12") > "123");
  EXPECT_TRUE(StaticString("123") > "12");
  EXPECT_FALSE(StaticString("123") < "12");
  EXPECT_TRUE(StaticString("122") < "123");
  EXPECT_FALSE(StaticString("122") > "123");
  EXPECT_TRUE(StaticString("123") > "122");
  EXPECT_FALSE(StaticString("123") < "122");
}

TEST(tl_static_string, swap_non_member) {
  {
    StaticString<6> a("012345");
    StaticString<6> b("abcd");

    swap(a, b);

    EXPECT_STATIC_STR_EQ(a, "abcd");
    EXPECT_STATIC_STR_EQ(b, "012345");
  }
}

TEST(tl_static_string, erase_non_member) {
  using StaticString = StaticString<24>;

  {
    StaticString str("01234567890123456789");
    EXPECT_EQ(erase(str, '3'), 2);
    EXPECT_STATIC_STR_EQ(str, "012456789012456789");
  }
}

TEST(tl_static_string, erase_if) {
  using StaticString = StaticString<24>;

  {
    StaticString str("0123456789");
    EXPECT_EQ(erase_if(str, [](char x) { return (x - '0') % 2 == 0; }), 5);
    EXPECT_STATIC_STR_EQ(str, "13579");
  }
}

TEST(tl_static_string, PutToStream) {
  std::stringstream os;
  os << StaticString<24>("Hello, World!");
  EXPECT_EQ(os.str(), "Hello, World!");
}

TEST(tl_static_string, GetFromStream) {
  std::stringstream is("Hello, World!");
  StaticString<24> str;

  is >> str;
  EXPECT_EQ(str, "Hello,");

  EXPECT_EQ(is.peek(), ' ');

  is >> str;
  EXPECT_EQ(str, "World!");
}

////////////////////////////////////////////////////////////////////////////////
// Compile-time evaluations.

namespace {

using SmallString = StaticString<64>;

// Note about constexpr
// ====================
//
// In theory consteval qualifier should be used for the functions below to tell
// the compiler that the expression must be evaluated at compile time.
//
// However, using constexpr causes Apple Clang 14 to crash when building the
// project in the release mode. At the same time the debug mode works correctly.
//
// Seems to be clearly a compiler bug (it should either not crash in release
// mode, or tell what part of C++ standard is violated). The workaround is to
// use constexpr and ensure that evaluation happened at compile time by using
// static_assert().
//
// Additionally, it is probably a good idea to dun text sections of the binary
// to see that it does contain these strings. Although need to be careful with
// that and not be confused with strings possibly used as arguments.

constexpr auto MakeDefaultString() -> SmallString {
  SmallString small_string;
  return small_string;
}

constexpr auto MakeSimpleString() -> SmallString {
  SmallString small_string("Hello, Consteval World!");
  return small_string;
}

constexpr auto MakeStringWithOperations() -> SmallString {
  SmallString small_string;
  small_string += "Hello, ";
  small_string += "Complex Consteval";
  return small_string;
}

}  // namespace

TEST(tl_static_string, ConstEval) {
  // NOLINTNEXTLINE(readability-container-size-empty)
  static_assert(MakeDefaultString() == "");
  static_assert(MakeDefaultString().empty());
  static_assert(MakeSimpleString() == "Hello, Consteval World!");
  static_assert(MakeStringWithOperations() == "Hello, Complex Consteval");
}

////////////////////////////////////////////////////////////////////////////////
// Maintenance.

// Avoid accident of a reference test committed.
TEST(tl_static_string, IsReal) { EXPECT_EQ(USE_STD_STRING_REFERENCE, 0); }

}  // namespace test

}  // namespace tiny_lib::static_string
