// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

// A fixed capacity dynamically sized string.
//
// Static string implements C++ string API and used in-object storage of a
// static size. No allocations will happen be performed by the static string.
//
//
// Exceptions
// ==========
//
// General notes on exceptions:
//
//  - If an exception is thrown for any reason, functions have effect (strong
//    exception guarantee).
//
//  - If an operation would result in size() > max_size(), an std::length_error
//    exception is throw.
//
//
// Version history
// ===============
//
//   0.0.1-alpha    (28 Dec 2023)    First public release.

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <istream>
#include <iterator>
#include <locale>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>

// Semantic version of the tl_static_string library.
#define TL_STATIC_STRING_VERSION_MAJOR 0
#define TL_STATIC_STRING_VERSION_MINOR 0
#define TL_STATIC_STRING_VERSION_REVISION 1

// Namespace of the module.
// The outer name spaces which surrounds the ABI-version namespace.
#ifndef TL_STATIC_STRING_NAMESPACE
#  define TL_STATIC_STRING_NAMESPACE tiny_lib::static_string
#endif

// Helpers for TL_STATIC_STRING_VERSION_NAMESPACE.
//
// Typical extra indirection for such conversion to allow macro to be expanded
// before it is converted to string.
#define TL_STATIC_STRING_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)        \
  v_##id1##_##id2##_##id3
#define TL_STATIC_STRING_VERSION_NAMESPACE_CONCAT(id1, id2, id3)               \
  TL_STATIC_STRING_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)

// Constructs identifier suitable for namespace denoting the current library
// version.
//
// For example: TL_STATIC_STRING_VERSION_NAMESPACE -> v_0_1_9
#define TL_STATIC_STRING_VERSION_NAMESPACE                                     \
  TL_STATIC_STRING_VERSION_NAMESPACE_CONCAT(TL_STATIC_STRING_VERSION_MAJOR,    \
                                            TL_STATIC_STRING_VERSION_MINOR,    \
                                            TL_STATIC_STRING_VERSION_REVISION)

// Throw an exception of the given type ExceptionType if the expression is
// evaluated to a truthful value.
// The default implementation throws an ExceptionType(#expression) if the
// condition is met.
// If the default behavior is not suitable for the application it should define
// TL_STATIC_STRING_THROW_IF prior to including this header. The provided
// implementation is not to return, otherwise an undefined behavior of memory
// corruption will happen.
#if !defined(TL_STATIC_STRING_THROW_IF)
#  define TL_STATIC_STRING_THROW_IF(ExceptionType, expression)                 \
    internal::ThrowIf<ExceptionType>(expression, #expression)
#endif

// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace TL_STATIC_STRING_NAMESPACE {
inline namespace TL_STATIC_STRING_VERSION_NAMESPACE {

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

// A default implementation of the TL_STATIC_STRING_THROW_IF().
template <class Exception>
inline constexpr void ThrowIf(const bool expression_eval,
                              const char* expression_str) {
  if (expression_eval) {
    ThrowOrAbort<Exception>(expression_str);
  }
}

// SFINAE expression to check T satisfies LegacyInputIterator.
//
// TODO(Sergey): This check is not fully valid as the LegacyInputIterator also
// requires equality_comparable, but that one does not seem to be trivially
// checked.
template <class T>
using EnableIfLegacyInputIterator = std::enable_if_t<std::input_iterator<T>>;

// SFINAE expression to check StringViewLike can be used to initialize a
// basic_string_view.
template <class StringViewLike, class CharT, class Traits>
using EnableIfStringViewLike = std::enable_if_t<std::conjunction_v<
    std::is_convertible<const StringViewLike&,
                        std::basic_string_view<CharT, Traits>>,
    std::negation<std::is_convertible<const StringViewLike&, const CharT*>>>>;

}  // namespace internal

// The code follows the STL naming convention for easier interchangeability with
// the standard string type.
//
// NOLINTBEGIN(readability-identifier-naming)

template <class CharT, std::size_t N, class Traits = std::char_traits<CharT>>
class BasicStaticString {
 public:
  static_assert(N > 0);

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
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  //////////////////////////////////////////////////////////////////////////////
  // Constants.

  // In-class alias for the maximum capacity (excluding null-terminator).
  static constexpr size_type static_capacity = N;

  // This is a special value equal to the maximum value representable by the
  // type size_type. The exact meaning depends on context, but it is generally
  // used either as end of string indicator by the functions that expect a
  // string index or as the error indicator by the functions that return a
  // string index.
  static constexpr size_type npos = size_type(-1);

  //////////////////////////////////////////////////////////////////////////////
  // Member functions.

  // Default constructor.
  // Constructs empty string with zero size.
  constexpr BasicStaticString() noexcept { SetSizeUnchecked(0); }

  // Constructs the string with count copies of character ch.
  constexpr BasicStaticString(const size_type count, const CharT ch) {
    assign(count, ch);
  }

  // Constructs the string with a substring [pos, pos+count) of other.
  // If count == npos, if count is not specified, or if the requested substring
  // lasts past the end of the string, the resulting substring is
  // [pos, other.size()).
  // If `pos > str.size()`, std::out_of_range is thrown.
  constexpr BasicStaticString(const BasicStaticString& other, size_type pos) {
    assign(other, pos);
  }
  constexpr BasicStaticString(const BasicStaticString& other,
                              const size_type pos,
                              const size_type count) {
    assign(other, pos, count);
  }

  // Constructs the string with the first count characters of character string
  // pointed to by s. s can contain null characters. The length of the string is
  // count.
  // The behavior is undefined if [s, s + count) is not a valid range.
  constexpr BasicStaticString(const CharT* s, const size_type count) {
    assign(s, count);
  }

  // Constructs the string with the contents initialized with a copy of the
  // null-terminated character string pointed to by s. The length of the string
  // is determined by the first null character.
  // The behavior is undefined if [s, s + Traits::length(s)) is not a valid
  // range.
  constexpr BasicStaticString(const CharT* s) { assign(s); }

  // Constructs the string with the contents of the range [first, last).
  template <class InputIt,
            class = internal::EnableIfLegacyInputIterator<InputIt>>
  constexpr BasicStaticString(InputIt first, InputIt last) {
    assign(first, last);
  }

  // Copy constructor. Constructs the string with a copy of the contents of
  // other.
  constexpr BasicStaticString(const BasicStaticString& other) {
    assign(other.data(), other.size());
  }

  // Move constructor.
  // Constructs the string with the contents of other using move semantics.
  // other is left in valid, but unspecified state.
  constexpr BasicStaticString(BasicStaticString&& other) noexcept {
    assign(std::move(other));
  }

  // Constructs the string with the contents of the initializer list ilist.
  constexpr BasicStaticString(const std::initializer_list<CharT> ilist) {
    assign(ilist);
  }

  // Implicitly converts t to a string view sv as if by
  // `std::basic_string_view<CharT, Traits> sv = t;`, then initializes the
  // string with the contents of sv, as if by
  // `basic_string(sv.data(), sv.size())`.
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  explicit constexpr BasicStaticString(const StringViewLike& t) {
    const StringViewType sv = t;
    assign(sv.data(), sv.size());
  }

  // Implicitly converts t to a string view sv as if by
  // `std::basic_string_view<CharT, Traits> sv = t;`, then initializes the
  // string with the subrange [pos, pos + n) of sv as if by
  // `BasicStaticString(sv.substr(pos, n))`.
  // `If pos > sv.size()`, std::out_of_range is thrown.
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr BasicStaticString(const StringViewLike& t,
                              const size_type pos,
                              const size_type n = npos) {
    const StringViewType sv = t;
    assign(sv.substr(pos, n));
  }

  // BasicStaticString cannot be constructed from nullptr.
  BasicStaticString(std::nullptr_t) = delete;

  ~BasicStaticString() = default;

  // Replaces the contents with a copy of str.
  // If *this and str are the same object, this function has no effect.
  constexpr auto operator=(const BasicStaticString& str) -> BasicStaticString& {
    if (&str == this) {
      return *this;
    }
    return assign(str.data(), str.size());
  }

  // Replaces the contents with those of str using move semantics.
  // str is in a valid but unspecified state afterwards.
  constexpr auto operator=(BasicStaticString&& str) noexcept
      -> BasicStaticString& {
    if (&str == this) {
      return *this;
    }
    return assign(std::move(str));
  }

  // Replaces the contents with those of null-terminated character string
  // pointed to by s as if by `assign(s, Traits::length(s))`.
  constexpr auto operator=(const CharT* s) -> BasicStaticString& {
    return assign(s, traits_type::length(s));
  }

  // Replaces the contents with character ch as if by
  // `assign(std::addressof(ch), 1)`.
  constexpr auto operator=(const CharT ch) -> BasicStaticString& {
    return assign(std::addressof(ch), 1);
  }

  // Replaces the contents with those of the initializer list ilist as if by
  //  `assign(ilist.begin(), ilist.size())`.
  constexpr auto operator=(const std::initializer_list<CharT> ilist)
      -> BasicStaticString& {
    return assign(ilist.begin(), ilist.size());
  }

  // Implicitly converts t to a string view sv as if by
  // `std::basic_string_view<CharT, Traits> sv = t;`, then replaces the contents
  // with those of the sv as if by `assign(sv)`.
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto operator=(const StringViewLike& t) -> BasicStaticString& {
    const StringViewType sv = t;
    return assign(sv);
  }

  // Replaces the contents with count copies of character ch.
  constexpr auto assign(const size_type count, const CharT ch)
      -> BasicStaticString& {
    TL_STATIC_STRING_THROW_IF(std::length_error, count > max_size());

    SetSizeUnchecked(count);
    traits_type::assign(data(), count, ch);

    return *this;
  }

  // Replaces the contents with a copy of str. Equivalent to *this = str;
  constexpr auto assign(const BasicStaticString& str) -> BasicStaticString& {
    if (&str == this) {
      return *this;
    }

    // No size check is needed as the str is known to be in a consistent state.

    SetSizeUnchecked(str.size());

    // Copy the characters. The null-terminator is handled as part of
    // SetSizeUnchecked().
    traits_type::copy(data(), str.data(), str.size());

    return *this;
  }

  // Replaces the contents with a substring [pos, pos + count) of str.
  // If the requested substring lasts past the end of the string, or if
  // `count == npos`, the resulting substring is [pos, str.size()).
  // If `pos > str.size()`, std::out_of_range is thrown.
  constexpr auto assign(const BasicStaticString& str,
                        const size_type pos,
                        const size_type count = npos) -> BasicStaticString& {
    TL_STATIC_STRING_THROW_IF(std::out_of_range, pos > str.size());

    const size_type actual_count = str.LengthOfSubString(pos, count);

    TL_STATIC_STRING_THROW_IF(std::length_error, actual_count > max_size());

    SetSizeUnchecked(actual_count);

    traits_type::copy(data(), str.data() + pos, actual_count);

    return *this;
  }

  // Replaces the contents with those of str using move semantics.
  // Equivalent to *this = std::move(str). In particular, allocator propagation
  // may take place.
  // src is left in valid, but unspecified state.
  constexpr auto assign(BasicStaticString&& str) noexcept
      -> BasicStaticString& {
    if (&str == this) {
      return *this;
    }

    SetSizeUnchecked(str.size());
    traits_type::copy(data(), str.data(), str.size());

    return *this;
  }

  // Replaces the contents with copies of the characters in the range
  // [s, s+count).
  // This range can contain null characters.
  // The behavior is undefined if [s, s + count) is not a valid range.
  constexpr auto assign(const CharT* s, const size_type count)
      -> BasicStaticString& {
    TL_STATIC_STRING_THROW_IF(std::length_error, count > max_size());

    SetSizeUnchecked(count);
    traits_type::copy(data(), s, count);
    return *this;
  }

  // Replaces the contents with those of null-terminated character string
  // pointed to by s. The length of the string is determined by the first null
  // character using `Traits::length(s)`.
  // The behavior is undefined if [s, s + Traits::length(s)) is not a valid
  // range.
  constexpr auto assign(const CharT* s) -> BasicStaticString& {
    const size_type count = traits_type::length(s);

    TL_STATIC_STRING_THROW_IF(std::length_error, count > max_size());

    SetSizeUnchecked(count);
    assign(s, count);

    return *this;
  }

  // Replaces the contents with copies of the characters in the range
  // [first, last).
  template <class InputIt,
            class = internal::EnableIfLegacyInputIterator<InputIt>>
  constexpr auto assign(InputIt first, InputIt last) -> BasicStaticString& {
    const size_type count = std::distance(first, last);

    TL_STATIC_STRING_THROW_IF(std::length_error, count > max_size());

    SetSizeUnchecked(count);

    for (CharT* ch = data(); first != last; ++first, ++ch) {
      Traits::assign(*ch, *first);
    }

    return *this;
  }

  // Replaces the contents with those of the initializer list ilist.
  constexpr auto assign(const std::initializer_list<CharT> ilist)
      -> BasicStaticString& {
    return assign(ilist.begin(), ilist.size());
  }

  // Implicitly converts t to a string view sv as if by
  // `std::basic_string_view<CharT, Traits> sv = t;`, then replaces the contents
  // with those of sv, as if by `assign(sv.data(), sv.size())`.
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto assign(const StringViewLike& t) -> BasicStaticString& {
    const StringViewType sv = t;
    return assign(sv.data(), sv.size());
  }

  // Implicitly converts t to a string view sv as if by
  // `std::basic_string_view<CharT, Traits> sv = t;`, then replaces the contents
  // with the characters from the subview [pos, pos+count) of sv.
  // If the requested subview lasts past the end of sv, or if `count == npos`,
  // the resulting subview is [pos, sv.size()).
  // `If pos > sv.size()`, std::out_of_range is thrown.
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto assign(const StringViewLike& t,
                        const size_type pos,
                        const size_type count = npos) -> BasicStaticString& {
    const StringViewType sv = t;
    return assign(sv.data() + pos, LengthOfSubString(sv.size(), pos, count));
  }

  // Element access
  // ==============

  // Returns a reference to the character at specified location pos.
  // Bounds checking is performed, exception of type std::out_of_range will be
  // thrown on invalid access.
  constexpr auto at(const size_type pos) -> reference {
    TL_STATIC_STRING_THROW_IF(std::out_of_range, pos >= size());
    return data_[pos];
  }
  constexpr auto at(const size_type pos) const -> const_reference {
    TL_STATIC_STRING_THROW_IF(std::out_of_range, pos >= size());
    return data_[pos];
  }

  // Returns a pointer to a null-terminated character array with data equivalent
  // to those stored in the string.
  constexpr auto data() const noexcept -> const CharT* { return data_; }
  constexpr auto data() noexcept -> CharT* { return data_; }
  constexpr auto c_str() const noexcept -> const CharT* { return data_; }

  // Returns a reference to the character at specified location pos.
  // No bounds checking is performed. If pos > size(), the behavior is
  // undefined.
  // If pos == size(), a reference to the character with value CharT() (the null
  // character) is returned.
  // For the non-const version, the behavior is undefined if this character is
  // modified to any value other than CharT().
  constexpr auto operator[](const size_type pos) -> reference {
    return data_[pos];
  }
  constexpr auto operator[](const size_type pos) const -> const_reference {
    return data_[pos];
  }

  // Returns reference to the first character in the string.
  // The behavior is undefined if `empty() == true`.
  constexpr auto front() -> CharT& { return operator[](0); }
  constexpr auto front() const -> const CharT& { return operator[](0); }

  // Returns reference to the last character in the string.
  // The behavior is undefined if empty() == true.
  constexpr auto back() -> CharT& { return operator[](size() - 1); }
  constexpr auto back() const -> const CharT& { return operator[](size() - 1); }

  // Returns a std::basic_string_view, constructed as if by
  // std::basic_string_view<CharT, Traits>(data(), size())
  constexpr operator std::basic_string_view<CharT, Traits>() const noexcept {
    return std::basic_string_view<CharT, Traits>(data(), size());
  }

  // Iterators
  // =========

  // Returns an iterator to the first character of the string.
  // begin() returns a mutable or constant iterator, depending on the constness
  // of *this.
  // cbegin() always returns a constant iterator. It is equivalent to
  // const_cast<const BasicStaticString&>(*this).begin().
  constexpr auto begin() noexcept -> iterator { return data(); }
  constexpr auto begin() const noexcept -> const_iterator { return data(); }
  constexpr auto cbegin() const noexcept -> const_iterator { return data(); }

  // Returns an iterator to the character following the last character of the
  // string.
  constexpr auto end() noexcept -> iterator { return data() + size(); }
  constexpr auto end() const noexcept -> const_iterator {
    return data() + size();
  }
  constexpr auto cend() const noexcept -> const_iterator {
    return data() + size();
  }

  // Returns a reverse iterator to the first character of the reversed string.
  // It corresponds to the last character of the non-reversed string.
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
  // the reversed string. It corresponds to the character preceding the first
  // character of the non-reversed string.
  // This character acts as a placeholder, attempting to access it results in
  // undefined behavior.
  constexpr auto rend() noexcept -> reverse_iterator {
    return reverse_iterator{begin()};
  }
  constexpr auto rend() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator{cbegin()};
  }
  constexpr auto crend() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator{cbegin()};
  }

  // Capacity
  // ========

  // Checks if the string has no characters, i.e. whether size() == 0.
  [[nodiscard]] constexpr auto empty() const noexcept -> bool {
    return size() == 0;
  }

  // Returns the number of CharT elements in the string, i.e.
  // std::distance(begin(), end()).
  constexpr auto size() const noexcept -> size_type { return size_; }
  constexpr auto length() const noexcept -> size_type { return size(); };

  // Returns the maximum number of elements the string is able to hold due to
  // the static storage size.
  constexpr auto max_size() const noexcept -> size_type { return N; }

  // Informs the object of a planned change in size, so that it can manage the
  // storage allocation appropriately.
  // This is a compatibility with the std::string API, has no effect.
  constexpr void reserve(const size_type new_cap) {
    TL_STATIC_STRING_THROW_IF(std::length_error, new_cap > max_size());
  }

  // A call to reserve with no argument is a non-binding shrink-to-fit request.
  // After this call, capacity() has an unspecified value greater than or equal
  // to size().
  // This is a compatibility with the std::string API, has no effect.
  [[deprecated]] void reserve() {}

  // Returns the number of characters that the string has currently allocated
  // space for.
  constexpr auto capacity() const noexcept -> size_type { return max_size(); }

  // Requests the removal of unused capacity.
  // This is a compatibility with the std::string API, has no effect.
  constexpr void shrink_to_fit() {}

  // Operations
  // ==========

  // Removes all characters from the string as if by executing
  // `erase(begin(), end())`.
  // All pointers, references, and iterators are invalidated.
  constexpr void clear() noexcept { SetSizeUnchecked(0); }

  // Inserts count copies of character ch at the position index.
  // Throws std::out_of_range if `index > size()`.
  constexpr auto insert(const size_type index,
                        const size_type count,
                        const CharT ch) -> BasicStaticString& {
    insert(cbegin() + index, count, ch);
    return *this;
  }

  // Inserts null-terminated character string pointed to by s at the position
  // index. The length of the string is determined by the first null character
  // using `Traits::length(s)`.
  // Throws std::out_of_range if `index > size()`.
  constexpr auto insert(const size_type index, const CharT* s)
      -> BasicStaticString& {
    return insert(index, s, traits_type::length(s));
  }

  // Inserts the characters in the range [s, s+count) at the position index.
  // The range can contain null characters.
  // Throws std::out_of_range if `index > size()`.
  constexpr auto insert(const size_type index,
                        const CharT* s,
                        const size_type count) -> BasicStaticString& {
    const size_type current_size = size();
    CharT* current_data = data();

    assert(current_size <= max_size());
    TL_STATIC_STRING_THROW_IF(std::out_of_range, index > current_size);
    TL_STATIC_STRING_THROW_IF(std::length_error,
                              count > max_size() - current_size);

    if (count) {
      traits_type::move(&current_data[index + count],
                        &current_data[index],
                        current_size - index);
    }

    traits_type::copy(&current_data[index], s, count);

    SetSizeUnchecked(current_size + count);

    return *this;
  }

  // Inserts string str at the position index.
  // Throws std::out_of_range if `index > size()`.
  constexpr auto insert(const size_type index, const BasicStaticString& str)
      -> BasicStaticString& {
    return insert(index, str.data(), str.size());
  }

  // Inserts a string, obtained by `str.substr(index_str, count)` at the
  // position index.
  // Throws std::out_of_range if `index > size()` or if
  // `index_str > str.size()`.
  constexpr auto insert(const size_type index,
                        const BasicStaticString& str,
                        const size_type index_str,
                        const size_type count = npos) -> BasicStaticString& {
    return insert(
        index, str.data() + index_str, str.LengthOfSubString(index_str, count));
  }

  // Inserts character ch before the character pointed by pos
  // The behavior is undefined if the iterator is not valid.
  constexpr auto insert(const const_iterator pos, const CharT ch) -> iterator {
    return insert(pos, 1, ch);
  }

  // Inserts count copies of character ch before the element (if any) pointed by
  // pos.
  // The behavior is undefined if the iterator is not valid.
  constexpr auto insert(const const_iterator pos,
                        const size_type count,
                        const CharT ch) -> iterator {
    const size_type current_size = size();
    CharT* current_data = data();
    const size_type index = pos - current_data;

    assert(current_size <= max_size());
    TL_STATIC_STRING_THROW_IF(std::out_of_range, index > current_size);
    TL_STATIC_STRING_THROW_IF(std::length_error,
                              count > max_size() - current_size);

    if (count) {
      traits_type::move(&current_data[index + count],
                        &current_data[index],
                        current_size - index);
    }

    traits_type::assign(&current_data[index], count, ch);

    SetSizeUnchecked(current_size + count);

    return &current_data[index];
  }

  // Inserts characters from the range [first, last) before the element (if any)
  // pointed by pos.
  // // The behavior is undefined if either of the iterator is not valid.
  template <class InputIt,
            class = internal::EnableIfLegacyInputIterator<InputIt>>
  constexpr auto insert(const_iterator pos, InputIt first, InputIt last)
      -> iterator {
    const size_type current_size = size();
    CharT* current_data = data();
    const size_type index = pos - current_data;
    const size_type count = std::distance(first, last);

    assert(current_size <= max_size());
    TL_STATIC_STRING_THROW_IF(std::out_of_range, index > current_size);
    TL_STATIC_STRING_THROW_IF(std::length_error,
                              count > max_size() - current_size);

    if (count) {
      traits_type::move(&current_data[index + count],
                        &current_data[index],
                        current_size - index);
    }

    for (CharT* ch = current_data + index; first != last; ++first, ++ch) {
      Traits::assign(*ch, *first);
    }

    SetSizeUnchecked(current_size + count);

    return &current_data[index];
  }

  // Inserts elements from initializer list ilist before the element (if any)
  // pointed by pos
  constexpr auto insert(const const_iterator pos,
                        const std::initializer_list<CharT> ilist) -> iterator {
    return insert(pos, ilist.begin(), ilist.end());
  }

  // Implicitly converts t to a string view sv as if by
  // `std::basic_string_view<CharT, Traits> sv = t;`, then inserts the elements
  // from sv before the element (if any) pointed by pos, as if by
  // `insert(pos, sv.data(), sv.size())`.
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto insert(const size_type pos, const StringViewLike& t)
      -> BasicStaticString& {
    const StringViewType sv = t;
    return insert(pos, sv.data(), sv.size());
  }

  // Implicitly converts t to a string view sv as if by
  // `std::basic_string_view<CharT, Traits> sv = t;`, then inserts, before the
  // element (if any) pointed by pos, the characters from the subview
  // [index_str, index_str+count) of sv. If the requested subview lasts past
  // the end of sv, or if count == npos, the resulting subview is
  // [index_str, sv.size()).
  // If `index_str > sv.size()`, or if `index > size()`, std::out_of_range is
  // thrown.
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto insert(const size_type index,
                        const StringViewLike& t,
                        const size_type index_str,
                        const size_type count = npos) -> BasicStaticString& {
    const StringViewType sv = t;
    return insert(index,
                  sv.data() + index_str,
                  LengthOfSubString(sv.size(), index_str, count));
  }

  // Removes `std::min(count, size() - index)` characters starting at index.
  // Throws std::out_of_range if `index > size()`.
  constexpr auto erase(const size_type index = 0, const size_type count = npos)
      -> BasicStaticString& {
    const size_type current_size = size();

    TL_STATIC_STRING_THROW_IF(std::out_of_range, index > current_size);

    const size_type actual_count = LengthOfSubString(index, count);
    erase(begin() + index, begin() + index + actual_count);

    return *this;
  }

  // Removes the character at position.
  constexpr auto erase(const_iterator position) -> iterator {
    return erase(position, position + 1);
  }

  // Removes the characters in the range [first, last).
  constexpr auto erase(const const_iterator first, const const_iterator last)
      -> iterator {
    const size_type current_size = size();
    CharT* current_data = data();
    const size_type index = first - current_data;
    const size_type count = last - first;

    TL_STATIC_STRING_THROW_IF(std::out_of_range, index > current_size);

    const size_type actual_count = LengthOfSubString(index, count);
    const size_type suffix_count = current_size - index - actual_count;

    if (count) {
      traits_type::move(
          &current_data[index], &current_data[index + count], suffix_count);
    }

    SetSizeUnchecked(current_size - actual_count);

    return current_data + index;
  }

  constexpr void push_back(const CharT ch) {
    const size_type current_size = size();

    TL_STATIC_STRING_THROW_IF(std::length_error, current_size == max_size());

    traits_type::assign(data_[current_size], ch);

    SetSizeUnchecked(current_size + 1);
  }

  // Removes the last character from the string.
  // Equivalent to erase(end()-1). The behavior is undefined if the string is
  // empty.
  constexpr void pop_back() {
    assert(size());

    SetSizeUnchecked(size() - 1);
  }

  // Appends count copies of character ch.
  constexpr auto append(const size_type count, const CharT ch)
      -> BasicStaticString& {
    const size_type current_size = size();
    CharT* current_data = data();

    assert(current_size <= max_size());
    TL_STATIC_STRING_THROW_IF(std::length_error,
                              count > max_size() - current_size);

    traits_type::assign(&current_data[current_size], count, ch);

    SetSizeUnchecked(current_size + count);

    return *this;
  }

  // Appends string str.
  constexpr auto append(const BasicStaticString& str) -> BasicStaticString& {
    return append(str.data(), str.size());
  }

  // Appends a substring [pos, pos+count) of str. If the requested substring
  // lasts past the end of the string, or if `count == npos`, the appended
  // substring is [pos, size()).
  // If `pos > str.size()`, std::out_of_range is thrown.
  constexpr auto append(const BasicStaticString& str,
                        size_type pos,
                        size_type count = npos) -> BasicStaticString& {
    const size_type current_size = size();
    CharT* current_data = data();

    TL_STATIC_STRING_THROW_IF(std::out_of_range, pos > str.size());

    const size_type actual_count = str.LengthOfSubString(pos, count);

    assert(current_size <= max_size());
    TL_STATIC_STRING_THROW_IF(std::length_error,
                              actual_count > max_size() - current_size);

    traits_type::copy(
        current_data + current_size, str.data() + pos, actual_count);

    SetSizeUnchecked(current_size + actual_count);

    return *this;
  }

  // Appends characters in the range [s, s + count).
  // This range can contain null characters.
  constexpr auto append(const CharT* s, size_type count) -> BasicStaticString& {
    const size_type current_size = size();
    CharT* current_data = data();

    assert(current_size <= max_size());
    TL_STATIC_STRING_THROW_IF(std::length_error,
                              count > max_size() - current_size);

    traits_type::copy(&current_data[current_size], s, count);

    SetSizeUnchecked(current_size + count);

    return *this;
  }

  // Appends the null-terminated character string pointed to by s.
  // The length of the string is determined by the first null character using
  // `Traits::length(s)`.
  constexpr auto append(const CharT* s) -> BasicStaticString& {
    return append(s, traits_type::length(s));
  }

  // Appends characters in the range [first, last).
  template <class InputIt,
            class = internal::EnableIfLegacyInputIterator<InputIt>>
  constexpr auto append(InputIt first, InputIt last) -> BasicStaticString& {
    const size_type current_size = size();
    CharT* current_data = data();
    const size_type count = std::distance(first, last);

    assert(current_size <= max_size());
    TL_STATIC_STRING_THROW_IF(std::length_error,
                              count > max_size() - current_size);

    for (CharT* ch = current_data + current_size; first != last;
         ++first, ++ch) {
      Traits::assign(*ch, *first);
    }

    SetSizeUnchecked(current_size + count);

    return *this;
  }

  // Appends characters from the initializer list ilist.
  constexpr auto append(const std::initializer_list<CharT> ilist)
      -> BasicStaticString& {
    return append(ilist.begin(), ilist.end());
  }

  // Implicitly converts t to a string view sv as if by
  // `std::basic_string_view<CharT, Traits> sv = t;`, then appends all
  // characters from sv as if by `append(sv.data(), sv.size())`.
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto append(const StringViewLike& t) -> BasicStaticString& {
    const StringViewType sv = t;
    return append(sv.data(), sv.size());
  }

  // Implicitly converts t to a string view sv as if by
  // std::basic_string_view<CharT, Traits> sv = t;, then appends the characters
  // from the subview [pos, pos+count) of sv. If the requested subview extends
  // past the end of sv, or if count == npos, the appended subview is [pos,
  // sv.size()).
  // If `pos >= sv.size()`, std::out_of_range is thrown.
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto append(const StringViewLike& t,
                        const size_type pos,
                        const size_type count = npos) -> BasicStaticString& {
    const StringViewType sv = t;
    const size_type actual_count = LengthOfSubString(sv.size(), pos, count);

    return append(sv.data() + pos, actual_count);
  }

  // Appends string str.
  constexpr auto operator+=(const BasicStaticString& str)
      -> BasicStaticString& {
    return append(str);
  }

  // Appends character ch.
  constexpr auto operator+=(const CharT ch) -> BasicStaticString& {
    return append(1, ch);
  }

  // Appends the null-terminated character string pointed to by s.
  constexpr auto operator+=(const CharT* s) -> BasicStaticString& {
    return append(s);
  }

  // Appends characters in the initializer list ilist.
  constexpr auto operator+=(const std::initializer_list<CharT> ilist)
      -> BasicStaticString& {
    return append(ilist);
  }

  //  Implicitly converts t to a string view sv as if by
  // `std::basic_string_view<CharT, Traits> sv = t;`, then appends characters in
  // the string view sv as if by append(sv).
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto operator+=(const StringViewLike& t) -> BasicStaticString& {
    return append(t);
  }

  // Compares this string to str.
  constexpr auto compare(const BasicStaticString& str) const noexcept -> int {
    const StringViewType lhs = StringViewType(*this);
    const StringViewType rhs = StringViewType(str);
    return lhs.compare(rhs);
  }

  // Compares a [pos1, pos1+count1) substring of this string to str.
  // If `count1 > size()` - pos1 the substring is [pos1, size()).
  constexpr auto compare(const size_type pos1,
                         const size_type count1,
                         const BasicStaticString& str) const -> int {
    const StringViewType lhs = StringViewType(*this).substr(pos1, count1);
    const StringViewType rhs = StringViewType(str);
    return lhs.compare(rhs);
  }

  // Compares a [pos1, pos1+count1) substring of this string to a substring
  // [pos2, pos2+count2) of str. If `count1 > size() - pos1` the first substring
  // is [pos1, size()). Likewise, `count2 > str.size() - pos2` the second
  // substring is [pos2, str.size()).
  constexpr auto compare(size_type pos1,
                         size_type count1,
                         const BasicStaticString& str,
                         size_type pos2,
                         size_type count2 = npos) const -> int {
    const StringViewType lhs = StringViewType(*this).substr(pos1, count1);
    const StringViewType rhs = StringViewType(str).substr(pos2, count2);
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

  // Implicitly converts t to a string view sv as if by
  // `std::basic_string_view<CharT, Traits> sv = t;`, then compares this string
  // to sv.
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto compare(const StringViewLike& t) const noexcept -> int {
    const StringViewType lhs = StringViewType(*this);
    const StringViewType rhs = StringViewType(t);
    return lhs.compare(rhs);
  }

  // Implicitly converts t to a string view sv as if by
  // `std::basic_string_view<CharT, Traits> sv = t;`, then compares a
  // [pos1, pos1+count1) substring of this string to sv, as if by
  // `std::basic_string_view<CharT, Traits>(*this).substr(pos1,
  // count1).compare(sv)`.
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto compare(const size_type pos1,
                         const size_type count1,
                         const StringViewLike& t) const -> int {
    const StringViewType lhs = StringViewType(*this).substr(pos1, count1);
    const StringViewType rhs = StringViewType(t);
    return lhs.compare(rhs);
  }

  // Implicitly converts t to a string view sv as if by
  // `std::basic_string_view<CharT, Traits> sv = t;`, then compares a
  // [pos1, pos1+count1) substring of this string to a substring
  // [pos2, pos2+count2) of sv as if by `std::basic_string_view<CharT,
  // Traits>(*this).substr(pos1, count1).compare(sv.substr(pos2, count2))`.
  template <class StringViewLike>
  constexpr auto compare(const size_type pos1,
                         const size_type count1,
                         const StringViewLike& t,
                         const size_type pos2,
                         const size_type count2 = npos) const -> int {
    const StringViewType lhs = StringViewType(*this).substr(pos1, count1);
    const StringViewType rhs = StringViewType(t).substr(pos2, count2);
    return lhs.compare(rhs);
  }

  // Checks if the string begins with the given prefix. The prefix may be one of
  // the following:
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

  // Checks if the string ends with the given suffix. The suffix may be one of
  // the following:
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

  // Checks if the string contains the given substring denotes by a string view
  // sv.
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

  // Replaces the part of the string indicated by [pos, pos + count) with the
  // new string str.
  constexpr auto replace(size_type pos,
                         size_type count,
                         const BasicStaticString& str) -> BasicStaticString& {
    return replace(pos, count, str, 0, str.size());
  }

  // Replaces the part of the string indicated by [first, last) with the new
  // string str.
  constexpr auto replace(const_iterator first,
                         const_iterator last,
                         const BasicStaticString& str) -> BasicStaticString& {
    const size_type pos = first - data();
    const size_type count = last - first;
    return replace(pos, count, str);
  }

  // Replaces the part of the string indicated by [pos, pos + count) with a
  // substring [pos2, pos2 + count2) of str, except if `count2==npos` or if
  // would extend past `str.size()`, [pos2, str.size()) is used.
  constexpr auto replace(const size_type pos,
                         const size_type count,
                         const BasicStaticString& str,
                         const size_type pos2,
                         const size_type count2 = npos) -> BasicStaticString& {
    const size_type actual_count2 = str.LengthOfSubString(pos2, count2);

    TL_STATIC_STRING_THROW_IF(std::out_of_range, pos2 > actual_count2);

    return replace(pos, count, str.data() + pos2, actual_count2);
  }

  // Replaces the part of the string indicated by [first, last) with characters
  // in the range [first2, last2).
  template <class InputIt,
            class = internal::EnableIfLegacyInputIterator<InputIt>>
  constexpr auto replace(const_iterator first,
                         const_iterator last,
                         InputIt first2,
                         InputIt last2) -> BasicStaticString& {
    const size_type current_size = size();
    CharT* current_data = data();

    const size_type pos = first - current_data;

    // Clamp the substring count.
    // Seems to match std::string from g++ and clang++, and allows to avoid
    // integer overflow when calculating the new string size.
    const size_type actual_count =
        LengthOfSubString(first - current_data, last - first);

    const size_type actual_count2 = last2 - first2;
    const size_type new_size =
        NewSizeAfterReplace(pos, actual_count, actual_count2);

    if (actual_count != actual_count2) {
      traits_type::move(&current_data[pos + actual_count2],
                        &current_data[pos + actual_count],
                        current_size - pos - actual_count);
    }

    for (CharT* ch = current_data + pos; first2 != last2; ++first2, ++ch) {
      Traits::assign(*ch, *first2);
    }

    SetSizeUnchecked(new_size);

    return *this;
  }

  // Replaces the part of the string indicated by [pos, pos + count) with a
  // characters in the range `[cstr, cstr + count2);`.
  constexpr auto replace(const size_type pos,
                         const size_type count,
                         const CharT* cstr,
                         const size_type count2) -> BasicStaticString& {
    return replace(begin() + pos, begin() + pos + count, cstr, count2);
  }

  // Replaces the part of the string indicated by [first, last) with a
  // characters in the range `[cstr, cstr + count2);`.
  constexpr auto replace(const_iterator first,
                         const_iterator last,
                         const CharT* cstr,
                         size_type count2) -> BasicStaticString& {
    const size_type current_size = size();
    CharT* current_data = data();

    const size_type pos = first - current_data;
    const size_type count = last - first;

    // Clamp the substring count.
    // Seems to match std::string from g++ and clang++, and allows to avoid
    // integer overflow when calculating the new string size.
    const size_type actual_count = LengthOfSubString(pos, count);

    const size_type new_size = NewSizeAfterReplace(pos, actual_count, count2);

    if (count2 != actual_count) {
      traits_type::move(&current_data[pos + count2],
                        &current_data[pos + actual_count],
                        current_size - pos - actual_count);
    }

    traits_type::copy(&current_data[pos], cstr, count2);

    SetSizeUnchecked(new_size);

    return *this;
  }

  // Replaces the part of the string indicated by [pos, pos + count) with count2
  // copies of character ch.
  constexpr auto replace(const size_type pos,
                         const size_type count,
                         const size_type count2,
                         const CharT ch) -> BasicStaticString& {
    return replace(begin() + pos, begin() + pos + count, count2, ch);
  }

  // Replaces the part of the string indicated by [first, last) with count2
  // copies of character ch.
  constexpr auto replace(const_iterator first,
                         const_iterator last,
                         const size_type count2,
                         const CharT ch) -> BasicStaticString& {
    const size_type current_size = size();
    CharT* current_data = data();

    const size_type pos = first - current_data;
    const size_type count = last - first;

    // Clamp the substring count.
    // Seems to match std::string from g++ and clang++, and allows to avoid
    // integer overflow when calculating the new string size.
    const size_type actual_count = LengthOfSubString(pos, count);

    const size_type new_size = NewSizeAfterReplace(pos, actual_count, count2);

    if (count2 != actual_count) {
      traits_type::move(&current_data[pos + count2],
                        &current_data[pos + actual_count],
                        current_size - pos - actual_count);
    }

    traits_type::assign(&current_data[pos], count2, ch);

    SetSizeUnchecked(new_size);

    return *this;
  }

  // Replaces the part of the string indicated by [first, last) with characters
  // in the initializer list ilist.
  constexpr auto replace(const_iterator first,
                         const_iterator last,
                         std::initializer_list<CharT> ilist)
      -> BasicStaticString& {
    return replace(first, last, ilist.begin(), ilist.end());
  }

  // Replaces the part of the string indicated by [pos, pos + count) with
  // characters of a string view sv, converted from t as if by
  // `std::basic_string_view<CharT, Traits> sv = t;`.
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto replace(size_type pos,
                         size_type count,
                         const StringViewLike& t) -> BasicStaticString& {
    const std::basic_string_view<CharT, Traits> sv = t;
    return replace(pos, count, sv.data(), sv.size());
  }

  // Replaces the part of the string indicated by [first, last) with
  // characters of a string view sv, converted from t as if by
  // `std::basic_string_view<CharT, Traits> sv = t;`.
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto replace(const_iterator first,
                         const_iterator last,
                         const StringViewLike& t) -> BasicStaticString& {
    const std::basic_string_view<CharT, Traits> sv = t;
    return replace(first, last, sv.data(), sv.size());
  }

  // Replaces the part of the string indicated by [first, last) with
  // subview [pos2, pos2 + count2) of a string view sv, converted from t as if
  // by `std::basic_string_view<CharT, Traits> sv = t;`, except if
  // `count2==npos` or if it would extend past `sv.size()`, `[pos2, sv.size())`
  // is used.
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto replace(const size_type pos,
                         const size_type count,
                         const StringViewLike& t,
                         const size_type pos2,
                         const size_type count2 = npos) -> BasicStaticString& {
    const std::basic_string_view<CharT, Traits> sv = t;
    return replace(pos,
                   count,
                   sv.data() + pos2,
                   LengthOfSubString(sv.size(), pos2, count2));
  }

  // Returns a substring [pos, pos+count). If the requested substring extends
  // past the end of the string, i.e. the count is greater than `size() - pos`
  // (e.g. if count == npos), the returned substring is [pos, size()).
  constexpr auto substr(const size_type pos = 0,
                        const size_type count = npos) const
      -> BasicStaticString {
    const size_type actual_count = LengthOfSubString(pos, count);
    return BasicStaticString(data() + pos, actual_count);
  }

  // Copies a substring [pos, pos+count) to character string pointed to by dest.
  // If the requested substring lasts past the end of the string, or if count ==
  // npos, the copied substring is [pos, size()). The resulting character string
  // is not null-terminated.
  //
  // If `pos > size()`, std::out_of_range is thrown.
  constexpr auto copy(CharT* dest,
                      const size_type count,
                      const size_type pos = 0) const -> size_type {
    TL_STATIC_STRING_THROW_IF(std::out_of_range, pos > size());

    const size_type actual_count = LengthOfSubString(pos, count);

    traits_type::copy(dest, data() + pos, actual_count);

    return actual_count;
  }

  // Resizes the string to contain count characters.
  // If the current size is less than count, additional characters are appended
  // and are initialized to CharT() ('\0' if CharT is char).
  // Throws std::length_error if `count > max_size()`.
  constexpr void resize(const size_type count) { resize(count, '\0'); }

  // Resizes the string to contain count characters.
  // If the current size is less than count, additional characters are appended
  // and are initialized to ch.
  // Throws std::length_error if `count > max_size()`.
  constexpr void resize(const size_type count, const CharT ch) {
    TL_STATIC_STRING_THROW_IF(std::length_error, count > max_size());

    const size_type current_size = size();

    if (count == current_size) {
      return;
    }

    if (count > current_size) {
      traits_type::assign(data() + current_size, count - current_size, ch);
    }

    SetSizeUnchecked(count);
  }

  // Resizes the string to contain at most count characters, using the
  // user-provided operation op to modify the possibly indeterminate contents
  // and set the length. This avoids the cost of initializing a suitably-sized
  // std::string when it is intended to be used as a char array to be populated
  // by, e.g., a C API call.
  template <class Operation>
  constexpr void resize_and_overwrite(const size_type count, Operation op) {
    TL_STATIC_STRING_THROW_IF(std::length_error, count > max_size());

    CharT* current_data = data();

    struct SizeAssigner {
      constexpr ~SizeAssigner() { str_ptr->SetSizeUnchecked(r); }

      BasicStaticString* str_ptr;
      size_type r;
    };

    SizeAssigner size_assigner;
    size_assigner.str_ptr = this;
    size_assigner.r = std::move(op)(current_data, count);

    assert(size_assigner.r <= count);
  }

  // Exchanges the contents of the string with those of other.
  // All iterators and references may be invalidated.
  constexpr void swap(BasicStaticString& other) noexcept {
    if (this == &other) {
      return;
    }

    // The string data is always fully initialized, so there is no worry about
    // accessing uninitialized part of the string.
    // Doing per-element sap is the easiest way to ensure the swap does not
    // throw.
    for (size_type i = 0; i < capacity(); ++i) {
      std::swap((*this)[i], other[i]);
    }

    std::swap(size_, other.size_);
  }

  // Search
  // ======

  // Finds the first substring equal to the given character sequence. Search
  // begins at pos, i.e. the found substring must not begin in a position
  // preceding pos.
  //
  // The following overloads are available:
  //  - Find the first substring equal to str
  //  - Find the first substring equal to the range [s, s+count).
  //    This range may contain null characters.
  //  - Find the first substring equal to the character string pointed to by s.
  //    The length of the string is determined by the first null character using
  //    Traits::length(s).
  //  - Find the first character ch (treated as a single-character substring.
  //  - Implicitly convert t to a string view sv as if by
  //    `std::basic_string_view<CharT, Traits> sv = t;`, then find the first
  //    substring equal to sv

  constexpr auto find(const BasicStaticString& str,
                      const size_type pos = 0) const noexcept -> size_type {
    return StringViewType(*this).find(StringViewType(str), pos);
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
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto find(const StringViewLike& t,
                      const size_type pos = 0) const noexcept -> size_type {
    return StringViewType(*this).find(StringViewType(t), pos);
  }

  // Finds the last substring equal to the given character sequence. Search
  // begins at pos, i.e. the found substring must not begin in a position
  // following pos. If npos or any value not smaller than size()-1 is passed as
  // pos, whole string will be searched.
  //
  // The following overloads are available:
  //  - Find the last substring equal to str.
  //  - Find the last substring equal to the range [s, s+count).
  //    This range can include null characters.
  //  - Find the last substring equal to the character string pointed to by s.
  //    The length of the string is determined by the first null character using
  //    Traits::length(s).
  //  - Find the last character equal to ch.
  //  - Implicitly convert t to a string view sv as if by
  //    `std::basic_string_view<CharT, Traits> sv = t;`, then find the last
  //    substring equal to the contents of sv.

  constexpr auto rfind(const BasicStaticString& str,
                       const size_type pos = npos) const noexcept -> size_type {
    return StringViewType(*this).rfind(StringViewType(str), pos);
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
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto rfind(const StringViewLike& t,
                       const size_type pos = npos) const noexcept -> size_type {
    return StringViewType(*this).rfind(StringViewType(t), pos);
  }

  // Finds the first character equal to one of the characters in the given
  // character sequence. The search considers only the interval [pos, size()).
  // If the character is not present in the interval, npos will be returned.
  //
  // The following overloads are available:
  //  - Find the first character equal to one of the characters in str.
  //  - Find the first character equal to one of the characters in the range
  //    [s, s+count). This range can include null characters.
  //  - Find the first character equal to one of the characters in character
  //    string pointed to by s. The length of the string is determined by the
  //    first null character using Traits::length(s).
  //  - Find the first character equal to ch.
  //  - Implicitly convert t to a string view sv as if by
  //    `std::basic_string_view<CharT, Traits> sv = t;`, then find the first
  //    character equal to one of the characters in sv.

  constexpr auto find_first_of(const BasicStaticString& str,
                               const size_type pos = 0) const noexcept
      -> size_type {
    return StringViewType(*this).find_first_of(StringViewType(str), pos);
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
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto find_first_of(const StringViewLike& t,
                               const size_type pos = 0) const noexcept
      -> size_type {
    return StringViewType(*this).find_first_of(StringViewType(t), pos);
  }

  // Find the first character equal to none of the characters in the given
  // character sequence. The search considers only the interval [pos, size()).
  // If the character is not present in the interval, npos will be returned.
  //
  // The following overloads are available:
  //  - Find the first character equal to none of characters in str.
  //  - Find the first character equal to none of characters in range
  //    [s, s+count). This range can include null characters.
  //  - Find the first character equal to none of characters in character string
  //    pointed to by s. The length of the string is determined by the first
  //    null character using Traits::length(s).
  // -  Find the first character not equal to ch.
  // -  Implicitly convert t to a string view sv as if by
  //    `std::basic_string_view<CharT, Traits> sv = t;`, then find the first
  //    character equal to none of characters in sv.

  constexpr auto find_first_not_of(const BasicStaticString& str,
                                   const size_type pos = 0) const noexcept
      -> size_type {
    return StringViewType(*this).find_first_not_of(StringViewType(str), pos);
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
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto find_first_not_of(const StringViewLike& t,
                                   const size_type pos = 0) const noexcept
      -> size_type {
    return StringViewType(*this).find_first_not_of(StringViewType(t), pos);
  }

  // Finds the last character equal to one of characters in the given character
  // sequence. The exact search algorithm is not specified. The search considers
  // only the interval [0, pos]. If the character is not present in the
  // interval, npos will be returned.
  //
  // The following overloads are available:
  //  - Find the last character equal to one of characters in str.
  //  - Find the last character equal to one of characters in range
  //    [s, s + count). This range can include null characters.
  //  - Find the last character equal to one of characters in character string
  //    pointed to by s. The length of the string is determined by the first
  //    null character using Traits::length(s).
  //  - Find the last character equal to ch.
  //  - Implicitly convert t to a string view sv as if by
  //    `std::basic_string_view<CharT, Traits> sv = t;`, then find the last
  //    character equal to one of characters in sv.

  constexpr auto find_last_of(const BasicStaticString& str,
                              const size_type pos = npos) const noexcept
      -> size_type {
    return StringViewType(*this).find_last_of(StringViewType(str), pos);
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
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto find_last_of(const StringViewLike& t,
                              const size_type pos = npos) const noexcept
      -> size_type {
    return StringViewType(*this).find_last_of(StringViewType(t), pos);
  }

  // Finds the last character equal to none of the characters in the given
  // character sequence. The search considers only the interval [0, pos]. If the
  // character is not present in the interval, npos will be returned.
  //
  // The following overloads are available:
  //  - Find the last character equal to none of characters in str.
  //  - Find the last character equal to none of characters in the range
  //    [s, s + count). This range can include null characters.
  //  - Find the last character equal to none of characters in character string
  //    pointed to by s. The length of the string is determined by the first
  //    null character using Traits::length(s).
  //  - Finds the last character not equal to ch.
  //  - Implicitly converts t to a string view sv as if by
  //    `std::basic_string_view<CharT, Traits> sv = t;`, then finds the last
  //    character equal to none of characters in sv.

  constexpr auto find_last_not_of(const BasicStaticString& str,
                                  const size_type pos = npos) const noexcept
      -> size_type {
    return StringViewType(*this).find_last_not_of(StringViewType(str), pos);
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
  template <
      class StringViewLike,
      class = internal::EnableIfStringViewLike<StringViewLike, CharT, Traits>>
  constexpr auto find_last_not_of(const StringViewLike& t,
                                  const size_type pos = npos) const noexcept
      -> size_type {
    return StringViewType(*this).find_last_not_of(StringViewType(t), pos);
  }

 private:
  // The string view type matching this string.
  using StringViewType = std::basic_string_view<CharT, Traits>;

  // Resizes the string to contain count characters without initializing the new
  // characters but ensuring that the string is null-terminated after the count
  // characters.
  //
  // No size check is happening here: it is up to the caller to handle overflow
  // when it occurs.
  constexpr void SetSizeUnchecked(const size_type count) {
    assert(count <= max_size());

    size_ = count;
    traits_type::assign(data_[size_], value_type());
  }

  // An actual length which result of substr(pos, count) will have.
  // If `pos > size()`, std::out_of_range is thrown.
  constexpr auto LengthOfSubString(const size_type pos,
                                   const size_type count) const -> size_type {
    return LengthOfSubString(size(), pos, count);
  }

  // An actual length which result of substr(pos, count) will have of a string
  // of the original given size.
  // If `pos > size`, std::out_of_range is thrown.
  static constexpr auto LengthOfSubString(const size_type size,
                                          const size_type pos,
                                          const size_type count) -> size_type {
    TL_STATIC_STRING_THROW_IF(std::out_of_range, pos > size);

    const size_type remaining_count = size - pos;
    return count > remaining_count ? remaining_count : count;
  }

  // Get new size after replacing substring indicated by [pos, pos + cout) with
  // a string of length count2.
  // The count values are expected to be clamped to their corresponding string
  // sizes.
  // Throws std::out_of_range if `pos > size()`.
  constexpr auto NewSizeAfterReplace(const size_type pos,
                                     const size_type count,
                                     const size_type count2) {
    const size_type current_size = size();

    assert(current_size > count);
    assert(current_size <= max_size());

    TL_STATIC_STRING_THROW_IF(std::out_of_range, pos > current_size);
    TL_STATIC_STRING_THROW_IF(std::length_error,
                              count2 > max_size() - current_size + count);

    return current_size - count + count2;
  }

  // NOTE: Initializing the entire storage seems a bit redundant, but this is
  // required with Apple Clang 14.0.0 to be able to create static string from
  // a const-eval function.
  // NOTE: The swap() relies on the fact that the data is fully initialized.
  CharT data_[N + 1]{};  // NOLINT(modernize-avoid-c-arrays)

  size_type size_{0};
};

////////////////////////////////////////////////////////////////////////////////
// Non-member functions.

// Returns a string containing characters from lhs followed by the characters
// from rhs.

template <std::size_t N, class CharT, class Traits>
constexpr auto operator+(const BasicStaticString<CharT, N, Traits>& lhs,
                         const BasicStaticString<CharT, N, Traits>& rhs)
    -> BasicStaticString<CharT, N, Traits> {
  BasicStaticString<CharT, N, Traits> result(lhs);
  result += rhs;
  return result;
}

template <std::size_t N, class CharT, class Traits>
constexpr auto operator+(const BasicStaticString<CharT, N, Traits>& lhs,
                         const CharT* rhs)
    -> BasicStaticString<CharT, N, Traits> {
  BasicStaticString<CharT, N, Traits> result(lhs);
  result += rhs;
  return result;
}

template <std::size_t N, class CharT, class Traits>
constexpr auto operator+(const BasicStaticString<CharT, N, Traits>& lhs,
                         const CharT rhs)
    -> BasicStaticString<CharT, N, Traits> {
  BasicStaticString<CharT, N, Traits> result(lhs);
  result += rhs;
  return result;
}

template <std::size_t N, class CharT, class Traits>
constexpr auto operator+(const CharT* lhs,
                         const BasicStaticString<CharT, N, Traits>& rhs)
    -> BasicStaticString<CharT, N, Traits> {
  BasicStaticString<CharT, N, Traits> result(lhs);
  result += rhs;
  return result;
}

template <std::size_t N, class CharT, class Traits>
constexpr auto operator+(const CharT lhs,
                         const BasicStaticString<CharT, N, Traits>& rhs)
    -> BasicStaticString<CharT, N, Traits> {
  BasicStaticString<CharT, N, Traits> result(1, lhs);
  result += rhs;
  return result;
}

template <std::size_t N, class CharT, class Traits>
constexpr auto operator+(BasicStaticString<CharT, N, Traits>&& lhs,
                         BasicStaticString<CharT, N, Traits>&& rhs)
    -> BasicStaticString<CharT, N, Traits> {
  BasicStaticString<CharT, N, Traits> result = std::move(lhs);
  result += std::move(rhs);
  return result;
}

template <std::size_t N, class CharT, class Traits>
constexpr auto operator+(BasicStaticString<CharT, N, Traits>&& lhs,
                         const BasicStaticString<CharT, N, Traits>& rhs)
    -> BasicStaticString<CharT, N, Traits> {
  BasicStaticString<CharT, N, Traits> result = std::move(lhs);
  result += rhs;
  return result;
}

template <std::size_t N, class CharT, class Traits>
constexpr auto operator+(BasicStaticString<CharT, N, Traits>&& lhs,
                         const CharT* rhs)
    -> BasicStaticString<CharT, N, Traits> {
  BasicStaticString<CharT, N, Traits> result = std::move(lhs);
  result += rhs;
  return result;
}

template <std::size_t N, class CharT, class Traits>
constexpr auto operator+(BasicStaticString<CharT, N, Traits>&& lhs,
                         const CharT rhs)
    -> BasicStaticString<CharT, N, Traits> {
  BasicStaticString<CharT, N, Traits> result = std::move(lhs);
  result += rhs;
  return result;
}

template <std::size_t N, class CharT, class Traits>
constexpr auto operator+(const BasicStaticString<CharT, N, Traits>& lhs,
                         BasicStaticString<CharT, N, Traits>&& rhs)
    -> BasicStaticString<CharT, N, Traits> {
  BasicStaticString<CharT, N, Traits> result(lhs);
  result += rhs;
  return result;
}

template <std::size_t N, class CharT, class Traits>
constexpr auto operator+(const CharT* lhs,
                         BasicStaticString<CharT, N, Traits>&& rhs)
    -> BasicStaticString<CharT, N, Traits> {
  BasicStaticString<CharT, N, Traits> result(lhs);
  result += rhs;
  return result;
}

template <std::size_t N, class CharT, class Traits>
constexpr auto operator+(const CharT lhs,
                         BasicStaticString<CharT, N, Traits>&& rhs)
    -> BasicStaticString<CharT, N, Traits> {
  BasicStaticString<CharT, N, Traits> result(1, lhs);
  result += rhs;
  return result;
}

// Compare two BasicStaticString objects.
template <std::size_t N, class CharT, class Traits>
constexpr auto operator==(
    const BasicStaticString<CharT, N, Traits>& lhs,
    const BasicStaticString<CharT, N, Traits>& rhs) noexcept -> bool {
  return lhs.compare(rhs) == 0;
}

template <std::size_t N, class CharT, class Traits>
constexpr auto operator<=>(
    const BasicStaticString<CharT, N, Traits>& lhs,
    const BasicStaticString<CharT, N, Traits>& rhs) noexcept {
  return lhs.compare(rhs) <=> 0;
}

// Compare a BasicStaticString object and null-terminated array of CharT.
template <std::size_t N, class CharT, class Traits>
constexpr auto operator==(const BasicStaticString<CharT, N, Traits>& lhs,
                          const CharT* rhs) -> bool {
  return lhs.compare(rhs) == 0;
}

template <std::size_t N, class CharT, class Traits>
constexpr auto operator<=>(const BasicStaticString<CharT, N, Traits>& lhs,
                           const CharT* rhs) noexcept {
  return lhs.compare(rhs) <=> 0;
}

// Specializes the swap() algorithm for BasicStaticString.
// Swaps the contents of lhs and rhs. Equivalent to `lhs.swap(rhs)`.
template <std::size_t N, class CharT, class Traits>
constexpr void swap(BasicStaticString<CharT, N, Traits>& lhs,
                    BasicStaticString<CharT, N, Traits>& rhs) noexcept {
  lhs.swap(rhs);
}

// Erases all elements that compare equal to value from the container.
// Returns the number of erased elements.
template <std::size_t N, class CharT, class Traits, class U>
constexpr auto erase(BasicStaticString<CharT, N, Traits>& c, const U& value) ->
    typename BasicStaticString<CharT, N, Traits>::size_type {
  auto* it = std::remove(c.begin(), c.end(), value);
  auto r = std::distance(it, c.end());
  c.erase(it, c.end());
  return r;
}

// Erases all elements that satisfy the predicate pred from the container.
// Returns the number of erased elements.
template <std::size_t N, class CharT, class Traits, class Pred>
constexpr auto erase_if(BasicStaticString<CharT, N, Traits>& c, Pred pred) ->
    typename BasicStaticString<CharT, N, Traits>::size_type {
  auto* it = std::remove_if(c.begin(), c.end(), pred);
  auto r = std::distance(it, c.end());
  c.erase(it, c.end());
  return r;
}

// Input/output
// ============

template <std::size_t N, class CharT, class Traits>
auto operator<<(std::basic_ostream<CharT, Traits>& os,
                const BasicStaticString<CharT, N, Traits>& str)
    -> std::basic_ostream<CharT, Traits>& {
  os << std::basic_string_view<CharT, Traits>(str);
  return os;
}

template <std::size_t N, class CharT, class Traits>
auto operator>>(std::basic_istream<CharT, Traits>& is,
                BasicStaticString<CharT, N, Traits>& str)
    -> std::basic_istream<CharT, Traits>& {
  using StreamIntType = typename std::basic_istream<CharT, Traits>::int_type;
  const typename BasicStaticString<CharT, N, Traits>::size_type max_size =
      str.max_size();

  const std::locale locale = is.getloc();
  const std::streamsize width = is.width();

  // Skip leading whitespace.
  if (is.flags() & std::ios_base::skipws) {
    while (!is.eof()) {
      const StreamIntType next_char = is.peek();
      if (next_char == Traits::eof()) {
        break;
      }
      if (!std::isspace(CharT(next_char), locale)) {
        break;
      }
      is.get();
    }
  }

  str.erase();

  std::streamsize num_characters_to_read = width ? width : max_size;
  while (!is.eof()) {
    if (num_characters_to_read == 0) {
      break;
    }

    const StreamIntType next_char = is.peek();
    if (next_char == Traits::eof()) {
      break;
    }
    if (std::isspace(CharT(next_char), locale)) {
      break;
    }

    const StreamIntType c = is.get();
    str.append(1, CharT(c));

    --num_characters_to_read;
  }

  return is;
}

// TODO(sergey): Implement getline()

// NOLINTEND(readability-identifier-naming)

////////////////////////////////////////////////////////////////////////////////
// Type definitions for common character types.

template <std::size_t N>
using StaticString = BasicStaticString<char, N, std::char_traits<char>>;

}  // namespace TL_STATIC_STRING_VERSION_NAMESPACE
}  // namespace TL_STATIC_STRING_NAMESPACE

#undef TL_STATIC_STRING_VERSION_MAJOR
#undef TL_STATIC_STRING_VERSION_MINOR
#undef TL_STATIC_STRING_VERSION_REVISION

#undef TL_STATIC_STRING_NAMESPACE

#undef TL_STATIC_STRING_VERSION_NAMESPACE_CONCAT_HELPER
#undef TL_STATIC_STRING_VERSION_NAMESPACE_CONCAT
#undef TL_STATIC_STRING_VERSION_NAMESPACE

#undef TL_STATIC_STRING_THROW_IF
