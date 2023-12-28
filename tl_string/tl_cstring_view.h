// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

// CStringView - a C compatible string_view adapter.
//
// This is a non-owning reference to a null-terminated sequence of characters.
//
// It has a higher footprint than regular `char*` due to the fact that it stores
// the size (just similar to the string_view), but it allows to keep track of
// possible out of bounds access, and allows to cheaply access the length of the
// string.
//
// It is inspired by
//
//   Andrew Tomazos
//   std::cstring_view - a C compatible std::string_view adapter
//   https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1402r0.pdf
//
//   Blender's StringRefNull
//   https://projects.blender.org/blender/blender/src/branch/main/source/blender/blenlib/BLI_string_ref.hh
//
// Exceptions
// ==========
//
// General notes on exceptions:
//
//  - If an exception is thrown for any reason, functions have effect (strong
//    exception guarantee).
//
//
// Version history
// ===============
//
//   0.0.1-alpha    (28 Dec 2023)    First public release.

#pragma once

#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

// Semantic version of the tl_cstring_view library.
#define TL_CSTRING_VIEW_VERSION_MAJOR 0
#define TL_CSTRING_VIEW_VERSION_MINOR 0
#define TL_CSTRING_VIEW_VERSION_REVISION 1

// Namespace of the module.
// The outer name spaces which surrounds the ABI-version namespace.
#ifndef TL_CSTRING_VIEW_NAMESPACE
#  define TL_CSTRING_VIEW_NAMESPACE tiny_lib::cstring_view
#endif

// Helpers for TL_CSTRING_VIEW_VERSION_NAMESPACE.
//
// Typical extra indirection for such conversion to allow macro to be expanded
// before it is converted to string.
#define TL_CSTRING_VIEW_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)         \
  v_##id1##_##id2##_##id3
#define TL_CSTRING_VIEW_VERSION_NAMESPACE_CONCAT(id1, id2, id3)                \
  TL_CSTRING_VIEW_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)

// Constructs identifier suitable for namespace denoting the current library
// version.
//
// For example: TL_CSTRING_VIEW_VERSION_NAMESPACE -> v_0_1_9
#define TL_CSTRING_VIEW_VERSION_NAMESPACE                                      \
  TL_CSTRING_VIEW_VERSION_NAMESPACE_CONCAT(TL_CSTRING_VIEW_VERSION_MAJOR,      \
                                           TL_CSTRING_VIEW_VERSION_MINOR,      \
                                           TL_CSTRING_VIEW_VERSION_REVISION)

// Throw an exception of the given type ExceptionType if the expression is
// evaluated to a truthful value.
// The default implementation throws an ExceptionType(#expression) if the
// condition is met.
// If the default behavior is not suitable for the application it should define
// TL_CSTRING_VIEW_THROW_IF prior to including this header. The provided
// implementation is not to return, otherwise an undefined behavior of memory
// corruption will happen.
#if !defined(TL_CSTRING_VIEW_THROW_IF)
#  define TL_CSTRING_VIEW_THROW_IF(ExceptionType, expression)                  \
    internal::ThrowIf<ExceptionType>(expression, #expression)
#endif

// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace TL_CSTRING_VIEW_NAMESPACE {
inline namespace TL_CSTRING_VIEW_VERSION_NAMESPACE {

namespace internal {

// If the exceptions are enabled throws a new exception of the given type
// passing the given expression_str to its constructor as a reason. If the
// exceptions are disabled aborts the program execution.
template <class Exception>
[[noreturn]] inline constexpr void ThrowOrAbort(const char* expression_str) {
#if defined(__cpp_exceptions) && __cpp_exceptions >= 199711L
  throw Exception(expression_str);
#else
  (void)expression_str;
  __builtin_abort();
#endif
}

// A default implementation of the TL_CSTRING_VIEW_THROW_IF().
template <class Exception>
inline constexpr void ThrowIf(const bool expression_eval,
                              const char* expression_str) {
  if (expression_eval) {
    ThrowOrAbort<Exception>(expression_str);
  }
}

// SFINAE expression to check StringViewLike can be used to initialize a
// basic_string_view.
template <class StringViewLike, class CharT, class Traits>
using EnableIfStringViewLike = std::enable_if_t<std::conjunction_v<
    std::is_convertible<const StringViewLike&,
                        std::basic_string_view<CharT, Traits>>,
    std::negation<std::is_convertible<const StringViewLike&, const CharT*>>>>;

// SFINAE expression to check StringLike implements string-like interface from
// which the null-terminated string can be created.
template <class StringLike, class CharT, class Traits>
using EnableIfIsStringLike = std::enable_if_t<std::conjunction_v<
    std::is_same<CharT, typename StringLike::value_type>,
    std::is_same<Traits, typename StringLike::traits_type>,
    std::is_same<const CharT*,
                 decltype((std::declval<StringLike>()).c_str())>>>;

}  // namespace internal

// The code follows the STL naming convention for easier interchangeability with
// the standard string type.
//
// NOLINTBEGIN(readability-identifier-naming)

template <class CharT, class Traits = std::char_traits<CharT>>
class BasicCStringView {
 public:
  //////////////////////////////////////////////////////////////////////////////
  // Member types.

  using traits_type = Traits;
  using value_type = CharT;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using iterator = const_pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  //////////////////////////////////////////////////////////////////////////////
  // Constants.

  // This is a special value equal to the maximum value representable by the
  // type size_type. The exact meaning depends on context, but it is generally
  // used either as end of string indicator by the functions that expect a
  // string index or as the error indicator by the functions that return a
  // string index.
  static constexpr size_type npos = size_type(-1);

  // Constructor and assignment
  // ==========================

  // Default constructor.
  //
  // Constructs an empty BasicCStringView. After construction, data() is equal
  // to nullptr, and size() is equal to ​0​.
  constexpr BasicCStringView() noexcept = default;

  // Copy constructor.
  //
  // Constructs a view of the same content as other. After construction, data()
  // is equal to other.data(), and size() is equal to other.size().
  constexpr BasicCStringView(const BasicCStringView& other) noexcept = default;

  // Constructs a view of the null-terminated character string pointed to by s,
  // not including the terminating null character. The length of the view is
  // determined as if by `Traits::length(s)`. The behavior is undefined if
  // `[s, s+Traits::length(s))` is not a valid range. After construction, data()
  // is equal to s, and size() is equal to `Traits::length(s)`.
  constexpr inline BasicCStringView(const CharT* s)
      : data_(s), size_(Traits::length(s)) {}

  template <class T, class = internal::EnableIfIsStringLike<T, CharT, Traits>>
  constexpr inline BasicCStringView(const T& t) noexcept
      : data_(t.c_str()), size_(t.size()) {}

  constexpr BasicCStringView(std::nullptr_t) = delete;

  // Replaces the view with that of view.
  constexpr inline auto operator=(const BasicCStringView& view) noexcept
      -> BasicCStringView& = default;

  // Iterators
  // =========

  // Returns an iterator to the first character of the string view.
  // begin() returns a mutable or constant iterator, depending on the constness
  // of *this.
  // cbegin() always returns a constant iterator. It is equivalent to
  // const_cast<const BasicCStringView>(*this).begin().
  constexpr auto begin() noexcept -> iterator { return data(); }
  constexpr auto begin() const noexcept -> const_iterator { return data(); }
  constexpr auto cbegin() const noexcept -> const_iterator { return data(); }

  // Returns an iterator to the character following the last character of the
  // string view.
  constexpr auto end() noexcept -> iterator { return data() + size(); }
  constexpr auto end() const noexcept -> const_iterator {
    return data() + size();
  }
  constexpr auto cend() const noexcept -> const_iterator {
    return data() + size();
  }

  // Returns a reverse iterator to the first character of the reversed string
  // view. It corresponds to the last character of the non-reversed string view.
  constexpr auto rbegin() noexcept -> reverse_iterator {
    return reverse_iterator{end()};
  }
  constexpr auto rbegin() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator{cend()};
  }
  constexpr auto crbegin() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator{cend()};
  }

  // Returns a reverse iterator to the character following the last character of
  // the reversed string view. It corresponds to the character preceding the
  // first character of the non-reversed string view. This character acts as a
  // placeholder, attempting to access it results in undefined behavior.
  constexpr auto rend() noexcept -> reverse_iterator {
    return reverse_iterator{begin()};
  }
  constexpr auto rend() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator{cbegin()};
  }
  constexpr auto crend() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator{cbegin()};
  }

  // Element access
  // ==============

  // Returns a reference to the character at specified location pos.
  // No bounds checking is performed. If pos > size(), the behavior is
  // undefined.
  // If pos == size(), a reference to the character with value CharT() (the null
  // character) is returned.
  constexpr auto operator[](const size_type pos) const -> const_reference {
    return data_[pos];
  }

  // Returns a reference to the character at specified location pos.
  // Bounds checking is performed, exception of type std::out_of_range will be
  // thrown on invalid access.
  constexpr auto at(const size_type pos) const -> const_reference {
    TL_CSTRING_VIEW_THROW_IF(std::out_of_range, pos >= size());
    return data_[pos];
  }

  // Returns reference to the first character in the string view.
  // The behavior is undefined if `empty() == true`.
  constexpr auto front() const -> const_reference { return operator[](0); }

  // Returns reference to the last character in the string view.
  // The behavior is undefined if empty() == true.
  constexpr auto back() const -> const_reference {
    return operator[](size() - 1);
  }

  // Returns a pointer to the underlying character array. The pointer is such
  // that the range [data(); data() + size()] is valid and the values in it
  // correspond to the values of the view.
  // The returned array is null-terminated, that is, data() and c_str() perform
  // the same function.
  // If empty() returns true, the pointer points to a single null character.
  constexpr auto data() const noexcept -> const_pointer { return data_; }

  // Returns a pointer to a null-terminated character array with data equivalent
  // to those stored in the string.
  //
  // The pointer is such that the range [c_str(); c_str() + size()] is valid and
  // the values in it correspond to the values stored in the string with an
  // additional null character after the last position.
  //
  // Writing to the character array accessed through c_str() is undefined
  // behavior.
  constexpr auto c_str() const noexcept -> const CharT* { return data_; }

  // Returns a std::basic_string_view, constructed as if by
  // `std::basic_string_view<CharT, Traits>(data(), size())`.
  constexpr operator std::basic_string_view<CharT, Traits>() const noexcept {
    return std::basic_string_view<CharT, Traits>(data(), size());
  }

  // Capacity
  // ========

  // Returns the number of CharT elements in the view.
  constexpr auto size() const noexcept -> size_type { return size_; }
  constexpr auto length() const noexcept -> size_type { return size_; }

  // The largest possible number of char-like objects that can be referred to by
  // a BasicCStringView.
  constexpr auto max_size() const noexcept -> size_type {
    return std::numeric_limits<size_type>::max();
  }

  // Checks if the view has no characters, i.e. whether size() == ​0​.
  [[nodiscard]] constexpr auto empty() const noexcept -> bool {
    return size() == 0;
  }

  // Modifiers
  // =========

  // Exchanges the view with that of v.
  constexpr void swap(BasicCStringView& v) noexcept {
    std::swap(data_, v.data_);
    std::swap(size_, v.size_);
  }

  // Operations
  // ==========

  // Compares this string view to v.
  constexpr auto compare(const BasicCStringView v) const noexcept -> int {
    const StringViewType lhs = StringViewType(*this);
    const StringViewType rhs = StringViewType(v);
    return lhs.compare(rhs);
  }

  // Compares a [pos1, pos1+count1) substring of this string view to v.
  // If `count1 > size()` - pos1 the substring is [pos1, size()).
  constexpr auto compare(const size_type pos1,
                         const size_type count1,
                         const BasicCStringView v) const -> int {
    const StringViewType lhs = StringViewType(*this).substr(pos1, count1);
    const StringViewType rhs = StringViewType(v);
    return lhs.compare(rhs);
  }

  // Compares a [pos1, pos1+count1) substring of this string view to a substring
  // [pos2, pos2+count2) of v. If `count1 > size() - pos1` the first substring
  // is [pos1, size()). Likewise, `count2 > v.size() - pos2` the second
  // substring is [pos2, v.size()).
  constexpr auto compare(size_type pos1,
                         size_type count1,
                         const BasicCStringView v,
                         size_type pos2,
                         size_type count2) const -> int {
    const StringViewType lhs = StringViewType(*this).substr(pos1, count1);
    const StringViewType rhs = StringViewType(v).substr(pos2, count2);
    return lhs.compare(rhs);
  }

  // Compares this string to the null-terminated character sequence beginning at
  // the character pointed to by s with length Traits::length(s).
  constexpr auto compare(const CharT* s) const -> int {
    const StringViewType lhs = StringViewType(*this);
    const StringViewType rhs = StringViewType(s);
    return lhs.compare(rhs);
  }

  // Compares a [pos1, pos1+count1) substring of this string to the
  // null-terminated character sequence beginning at the character pointed to by
  // s with length Traits::length(s). If `count1 > size() - pos1` the substring
  // is [pos1, size()).
  constexpr auto compare(size_type pos1, size_type count1, const CharT* s) const
      -> int {
    const StringViewType lhs = StringViewType(*this).substr(pos1, count1);
    const StringViewType rhs = StringViewType(s);
    return lhs.compare(rhs);
  }

  // Compares a [pos1, pos1+count1) substring of this string to the characters
  // in the range [s, s + count2). If count1 > size() - pos1 the substring is
  // [pos1, size()). (Note: the characters in the range [s, s + count2) may
  // include null characters).
  constexpr auto compare(size_type pos1,
                         size_type count1,
                         const CharT* s,
                         size_type count2) const -> int {
    const StringViewType lhs = StringViewType(*this).substr(pos1, count1);
    const StringViewType rhs = StringViewType(s).substr(0, count2);
    return lhs.compare(rhs);
  }

  // Checks if the string view begins with the given prefix. The prefix may be
  // one of the following:
  //   - a string view sv (which may be a result of implicit conversion from
  //       another BasicStaticString).
  //   - a single character c.
  //   - a null-terminated character string s.
  constexpr auto starts_with(
      const std::basic_string_view<CharT, Traits> sv) const noexcept -> bool {
    return StringViewType(*this).starts_with(sv);
  }
  constexpr auto starts_with(const CharT c) const noexcept -> bool {
    return StringViewType(*this).starts_with(c);
  }
  constexpr auto starts_with(const CharT* s) const -> bool {
    return StringViewType(*this).starts_with(s);
  }

  // Checks if the string view ends with the given suffix. The suffix may be one
  // of the following:
  //   - a string view sv (which may be a result of implicit conversion from
  //       another BasicStaticString).
  //   - a single character c.
  //   - a null-terminated character string s.
  constexpr auto ends_with(
      const std::basic_string_view<CharT, Traits> sv) const noexcept -> bool {
    return StringViewType(*this).ends_with(sv);
  }
  constexpr auto ends_with(const CharT c) const noexcept -> bool {
    return StringViewType(*this).ends_with(c);
  }
  constexpr auto ends_with(const CharT* s) const -> bool {
    return StringViewType(*this).ends_with(s);
  }

  // Checks if the string view contains the given substring denotes by a string
  // view sv.
  constexpr auto contains(
      const std::basic_string_view<CharT, Traits> sv) const noexcept -> bool {
    return find(sv) != npos;
  }

  // Checks if the string contains the given character.
  constexpr auto contains(const CharT c) const noexcept -> bool {
    return find(c) != npos;
  }

  // Checks if the string contains the given substring denotes by  a
  // null-terminated character string.
  constexpr auto contains(const CharT* s) const -> bool {
    return find(s) != npos;
  }

  // Search
  // ======

  // Finds the first substring equal to the given character sequence. Search
  // begins at pos, i.e. the found substring must not begin in a position
  // preceding pos.
  //
  // The following overloads are available:
  //  - Find the first substring equal to v
  //  - Find the first substring equal to the range [s, s+count).
  //    This range may contain null characters.
  //  - Find the first substring equal to the character string pointed to by s.
  //    The length of the string is determined by the first null character using
  //    Traits::length(s).
  //  - Find the first character ch (treated as a single-character substring.
  constexpr auto find(const std::basic_string_view<CharT, Traits> v,
                      const size_type pos = 0) const noexcept -> size_type {
    return StringViewType(*this).find(StringViewType(v), pos);
  }
  constexpr auto find(const CharT* s,
                      const size_type pos,
                      const size_type count) const -> size_type {
    return StringViewType(*this).find(s, pos, count);
  }
  constexpr auto find(const CharT* s, const size_type pos = 0) const
      -> size_type {
    return StringViewType(*this).find(s, pos);
  }
  constexpr auto find(const CharT ch, const size_type pos = 0) const noexcept
      -> size_type {
    return StringViewType(*this).find(ch, pos);
  }

  // Finds the last substring equal to the given character sequence. Search
  // begins at pos, i.e. the found substring must not begin in a position
  // following pos. If npos or any value not smaller than size()-1 is passed as
  // pos, whole string will be searched.
  //
  // The following overloads are available:
  //  - Find the last substring equal to v.
  //  - Find the last substring equal to the range [s, s+count).
  //    This range can include null characters.
  //  - Find the last substring equal to the character string pointed to by s.
  //    The length of the string is determined by the first null character using
  //    Traits::length(s).
  //  - Find the last character equal to ch.

  constexpr auto rfind(const std::basic_string_view<CharT, Traits> v,
                       const size_type pos = npos) const noexcept -> size_type {
    return StringViewType(*this).rfind(StringViewType(v), pos);
  }
  constexpr auto rfind(const CharT* s,
                       const size_type pos,
                       const size_type count) const -> size_type {
    return StringViewType(*this).rfind(s, pos, count);
  }
  constexpr auto rfind(const CharT* s, const size_type pos = npos) const
      -> size_type {
    return StringViewType(*this).rfind(s, pos);
  }
  constexpr auto rfind(const CharT ch,
                       const size_type pos = npos) const noexcept -> size_type {
    return StringViewType(*this).rfind(ch, pos);
  }

  // Finds the first character equal to one of the characters in the given
  // character sequence. The search considers only the interval [pos, size()).
  // If the character is not present in the interval, npos will be returned.
  //
  // The following overloads are available:
  //  - Find the first character equal to one of the characters in v.
  //  - Find the first character equal to one of the characters in the range
  //    [s, s+count). This range can include null characters.
  //  - Find the first character equal to one of the characters in character
  //    string pointed to by s. The length of the string is determined by the
  //    first null character using Traits::length(s).
  //  - Find the first character equal to ch.

  constexpr auto find_first_of(const std::basic_string_view<CharT, Traits> v,
                               const size_type pos = 0) const noexcept
      -> size_type {
    return StringViewType(*this).find_first_of(StringViewType(v), pos);
  }
  constexpr auto find_first_of(const CharT* s,
                               const size_type pos,
                               const size_type count) const -> size_type {
    return StringViewType(*this).find_first_of(s, pos, count);
  }
  constexpr auto find_first_of(const CharT* s, const size_type pos = 0) const
      -> size_type {
    return StringViewType(*this).find_first_of(s, pos);
  }
  constexpr auto find_first_of(const CharT ch,
                               const size_type pos = 0) const noexcept
      -> size_type {
    return StringViewType(*this).find_first_of(ch, pos);
  }

  // Find the first character equal to none of the characters in the given
  // character sequence. The search considers only the interval [pos, size()).
  // If the character is not present in the interval, npos will be returned.
  //
  // The following overloads are available:
  //  - Find the first character equal to none of characters in v.
  //  - Find the first character equal to none of characters in range
  //    [s, s+count). This range can include null characters.
  //  - Find the first character equal to none of characters in character string
  //    pointed to by s. The length of the string is determined by the first
  //    null character using Traits::length(s).
  // -  Find the first character not equal to ch.

  constexpr auto find_first_not_of(
      const std::basic_string_view<CharT, Traits> v,
      const size_type pos = 0) const noexcept -> size_type {
    return StringViewType(*this).find_first_not_of(StringViewType(v), pos);
  }
  constexpr auto find_first_not_of(const CharT* s,
                                   const size_type pos,
                                   const size_type count) const -> size_type {
    return StringViewType(*this).find_first_not_of(s, pos, count);
  }
  constexpr auto find_first_not_of(const CharT* s,
                                   const size_type pos = 0) const -> size_type {
    return StringViewType(*this).find_first_not_of(s, pos);
  }
  constexpr auto find_first_not_of(const CharT ch,
                                   const size_type pos = 0) const noexcept
      -> size_type {
    return StringViewType(*this).find_first_not_of(ch, pos);
  }

  // Finds the last character equal to one of characters in the given character
  // sequence. The exact search algorithm is not specified. The search considers
  // only the interval [0, pos]. If the character is not present in the
  // interval, npos will be returned.
  //
  // The following overloads are available:
  //  - Find the last character equal to one of characters in v.
  //  - Find the last character equal to one of characters in range
  //    [s, s + count). This range can include null characters.
  //  - Find the last character equal to one of characters in character string
  //    pointed to by s. The length of the string is determined by the first
  //    null character using Traits::length(s).
  //  - Find the last character equal to ch.

  constexpr auto find_last_of(const std::basic_string_view<CharT, Traits> v,
                              const size_type pos = npos) const noexcept
      -> size_type {
    return StringViewType(*this).find_last_of(StringViewType(v), pos);
  }
  constexpr auto find_last_of(const CharT* s,
                              const size_type pos,
                              const size_type count) const -> size_type {
    return StringViewType(*this).find_last_of(s, pos, count);
  }
  constexpr auto find_last_of(const CharT* s, const size_type pos = npos) const
      -> size_type {
    return StringViewType(*this).find_last_of(s, pos);
  }
  constexpr auto find_last_of(const CharT ch,
                              const size_type pos = npos) const noexcept
      -> size_type {
    return StringViewType(*this).find_last_of(ch, pos);
  }

  // Finds the last character equal to none of the characters in the given
  // character sequence. The search considers only the interval [0, pos]. If the
  // character is not present in the interval, npos will be returned.
  //
  // The following overloads are available:
  //  - Find the last character equal to none of characters in v.
  //  - Find the last character equal to none of characters in the range
  //    [s, s + count). This range can include null characters.
  //  - Find the last character equal to none of characters in character string
  //    pointed to by s. The length of the string is determined by the first
  //    null character using Traits::length(s).
  //  - Finds the last character not equal to ch.

  constexpr auto find_last_not_of(const std::basic_string_view<CharT, Traits> v,
                                  const size_type pos = npos) const noexcept
      -> size_type {
    return StringViewType(*this).find_last_not_of(StringViewType(v), pos);
  }
  constexpr auto find_last_not_of(const CharT* s,
                                  const size_type pos,
                                  const size_type count) const -> size_type {
    return StringViewType(*this).find_last_not_of(s, pos, count);
  }
  constexpr auto find_last_not_of(const CharT* s,
                                  const size_type pos = npos) const
      -> size_type {
    return StringViewType(*this).find_last_not_of(s, pos);
  }
  constexpr auto find_last_not_of(const CharT ch,
                                  const size_type pos = npos) const noexcept
      -> size_type {
    return StringViewType(*this).find_last_not_of(ch, pos);
  }

 private:
  // The string view type matching this string.
  using StringViewType = std::basic_string_view<CharT, Traits>;

  const CharT* data_{nullptr};
  size_type size_{0};
};

////////////////////////////////////////////////////////////////////////////////
// Non-member functions.

// Compare two BasicCStringView objects.
template <class CharT, class Traits>
constexpr auto operator==(const BasicCStringView<CharT, Traits> lhs,
                          const BasicCStringView<CharT, Traits> rhs) noexcept
    -> bool {
  return lhs.compare(rhs) == 0;
}

template <class CharT, class Traits>
constexpr auto operator<=>(const BasicCStringView<CharT, Traits> lhs,
                           const BasicCStringView<CharT, Traits> rhs) noexcept {
  return lhs.compare(rhs) <=> 0;
}

// Compare a BasicStaticString object and null-terminated array of CharT.
template <class CharT, class Traits>
constexpr auto operator==(const BasicCStringView<CharT, Traits> lhs,
                          const CharT* rhs) -> bool {
  return lhs.compare(rhs) == 0;
}

template <class CharT, class Traits>
constexpr auto operator<=>(const BasicCStringView<CharT, Traits> lhs,
                           const CharT* rhs) noexcept {
  return lhs.compare(rhs) <=> 0;
}

template <class CharT,
          class Traits,
          class T,
          class = internal::EnableIfStringViewLike<T, CharT, Traits>>
constexpr auto operator==(const BasicCStringView<CharT, Traits> lhs,
                          const T& rhs) -> bool {
  return lhs.compare(rhs) == 0;
}

template <class CharT,
          class Traits,
          class T,
          class = internal::EnableIfStringViewLike<T, CharT, Traits>>
constexpr auto operator<=>(const BasicCStringView<CharT, Traits> lhs,
                           const T& rhs) -> bool {
  return lhs.compare(rhs) <=> 0;
}

// Specializes the swap() algorithm for BasicStaticString.
// Swaps the contents of lhs and rhs. Equivalent to `lhs.swap(rhs)`.
template <class CharT, class Traits>
constexpr void swap(BasicCStringView<CharT, Traits>& lhs,
                    BasicCStringView<CharT, Traits>& rhs) noexcept {
  lhs.swap(rhs);
}

// Input/output
// ============

template <class CharT, class Traits>
auto operator<<(std::basic_ostream<CharT, Traits>& os,
                const BasicCStringView<CharT, Traits> v)
    -> std::basic_ostream<CharT, Traits>& {
  os << std::basic_string_view<CharT, Traits>(v);
  return os;
}

// NOLINTEND(readability-identifier-naming)

////////////////////////////////////////////////////////////////////////////////
// Type definitions for common character types.

using CStringView = BasicCStringView<char, std::char_traits<char>>;

}  // namespace TL_CSTRING_VIEW_VERSION_NAMESPACE
}  // namespace TL_CSTRING_VIEW_NAMESPACE

#undef TL_CSTRING_VIEW_VERSION_MAJOR
#undef TL_CSTRING_VIEW_VERSION_MINOR
#undef TL_CSTRING_VIEW_VERSION_REVISION

#undef TL_CSTRING_VIEW_VERSION_NAMESPACE

#undef TL_CSTRING_VIEW_VERSION_NAMESPACE_CONCAT_HELPER
#undef TL_CSTRING_VIEW_VERSION_NAMESPACE_CONCAT
#undef TL_CSTRING_VIEW_NAMESPACE

#undef TL_CSTRING_VIEW_THROW_IF
