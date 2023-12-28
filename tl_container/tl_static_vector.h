// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

// A fixed capacity dynamically sized vector.
//
// StaticVector implements C++ std::vector API and used in-object storage of a
// static size. No allocations will happen be performed by the static vector.
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
// When code is compiled without exceptions instead of throwing exception the
// code aborts the program execution.
//
//
// Version history
// ===============
//
//   0.0.1-alpha    (28 Dec 2023)    First public release.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <utility>

// Semantic version of the tl_static_vector library.
#define TL_STATIC_VECTOR_VERSION_MAJOR 0
#define TL_STATIC_VECTOR_VERSION_MINOR 0
#define TL_STATIC_VECTOR_VERSION_REVISION 1

// Namespace of the module.
// The outer name spaces which surrounds the ABI-version namespace.
#ifndef TL_STATIC_VECTOR_NAMESPACE
#  define TL_STATIC_VECTOR_NAMESPACE tiny_lib::static_vector
#endif

// Helpers for TL_STATIC_VECTOR_VERSION_NAMESPACE.
//
// Typical extra indirection for such conversion to allow macro to be expanded
// before it is converted to string.
#define TL_STATIC_VECTOR_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)        \
  v_##id1##_##id2##_##id3
#define TL_STATIC_VECTOR_VERSION_NAMESPACE_CONCAT(id1, id2, id3)               \
  TL_STATIC_VECTOR_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)

// Constructs identifier suitable for namespace denoting the current library
// version.
//
// For example: TL_STATIC_VECTOR_VERSION_NAMESPACE -> v_0_1_9
#define TL_STATIC_VECTOR_VERSION_NAMESPACE                                     \
  TL_STATIC_VECTOR_VERSION_NAMESPACE_CONCAT(TL_STATIC_VECTOR_VERSION_MAJOR,    \
                                            TL_STATIC_VECTOR_VERSION_MINOR,    \
                                            TL_STATIC_VECTOR_VERSION_REVISION)

// Throw an exception of the given type ExceptionType if the expression is
// evaluated to a truthful value.
// The default implementation throws an ExceptionType(#expression) if the
// condition is met.
// If the default behavior is not suitable for the application it should define
// TL_STATIC_VECTOR_THROW_IF prior to including this header. The provided
// implementation is not to return, otherwise an undefined behavior of memory
// corruption will happen.
#if !defined(TL_STATIC_VECTOR_THROW_IF)
#  define TL_STATIC_VECTOR_THROW_IF(ExceptionType, expression)                 \
    internal::ThrowIfOrAbort<ExceptionType>(expression, #expression)
#endif

// By default the debug build will execute extra code to ensure correctness of
// memory handling in terms of access to uninitialized memory.
//
// If this is undesired, define TL_STATIC_VECTOR_NO_MEMORY_DEBUG before
// including this header.
#if !defined(TL_STATIC_VECTOR_NO_MEMORY_DEBUG) && !defined(NDEBUG)
#  define TL_STATIC_VECTOR_USE_MEMORY_DEBUG 1
#else
#  define TL_STATIC_VECTOR_USE_MEMORY_DEBUG 0
#endif

// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace TL_STATIC_VECTOR_NAMESPACE {
inline namespace TL_STATIC_VECTOR_VERSION_NAMESPACE {

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

// A default implementation of the TL_STATIC_VECTOR_THROW_IF().
template <class Exception>
inline constexpr void ThrowIfOrAbort(const bool expression_eval,
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

}  // namespace internal

// The code follows the STL naming convention for easier interchangeability with
// the standard vector type.
//
// NOLINTBEGIN(readability-identifier-naming)

template <class T, std::size_t N>
class StaticVector {
 public:
  static_assert(N > 0);

  //////////////////////////////////////////////////////////////////////////////
  // Member types.

  using value_type = T;
  using size_type = size_t;
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

  //////////////////////////////////////////////////////////////////////////////
  // Member functions.

  // Default constructor.
  // Constructs an empty container.
  constexpr StaticVector() noexcept { MarkMemoryUninitialized(); }

  // Constructs the container with count copies of elements with value value.
  // Will throw std::length_error if the count > N.
  constexpr StaticVector(const size_type count, const T& value) {
    MarkMemoryUninitialized();
    assign(count, value);
  }

  // Constructs the container with count default-inserted instances of T.
  // No copies are made.
  constexpr explicit StaticVector(const size_type count) {
    MarkMemoryUninitialized();

    reserve(count);

    for (size_type i = 0; i < count; ++i) {
      new (GetElementPointer(i)) T();
    }

    size_ = count;
  }

  // Constructs the container with the contents of the range [first, last).
  template <class InputIt,
            class = internal::EnableIfLegacyInputIterator<InputIt>>
  constexpr StaticVector(InputIt first, InputIt last) {
    MarkMemoryUninitialized();

    assign(first, last);
  }

  // Copy constructor.
  // Constructs the container with the copy of the contents of other.
  constexpr StaticVector(const StaticVector& other) {
    MarkMemoryUninitialized();

    const size_type count = other.size();
    reserve(count);

    for (size_type i = 0; i < count; ++i) {
      new (GetElementPointer(i)) T(*other.GetElementPointer(i));
    }

    size_ = count;
  }

  // Move constructor.
  // Constructs the container with the contents of other using move semantics.
  // After the move, other is guaranteed to be empty().
  constexpr StaticVector(StaticVector&& other) noexcept {
    MarkMemoryUninitialized();

    const size_type count = other.size();

    for (size_type i = 0; i < count; ++i) {
      new (GetElementPointer(i)) T(std::move(*other.GetElementPointer(i)));
    }

    size_ = count;

    other.ResetOnMove();
  }

  // Constructs the container with the contents of the initializer list init.
  constexpr StaticVector(std::initializer_list<T> init)
      : StaticVector(init.begin(), init.end()) {}

  ~StaticVector() { clear(); }

  // Copy assignment operator.
  // Replaces the contents with a copy of the contents of other.
  constexpr auto operator=(const StaticVector& other) -> StaticVector& {
    if (this == &other) {
      return *this;
    }

    assign(other.begin(), other.end());

    return *this;
  }

  // Move assignment operator.
  // Replaces the contents with those of other using move semantics (i.e. the
  // data in other is moved from other into this container). other is in a valid
  // but unspecified state afterwards.
  constexpr auto operator=(StaticVector&& other) noexcept -> StaticVector& {
    if (this == &other) {
      return *this;
    }

    clear();

    const size_t count = other.size();

    for (size_type i = 0; i < count; ++i) {
      new (GetElementPointer(i)) T(std::move(*other.GetElementPointer(i)));
    }

    size_ = count;

    other.ResetOnMove();

    return *this;
  }

  // Replaces the contents with count copies of value value.
  constexpr void assign(size_type count, const T& value) {
    TL_STATIC_VECTOR_THROW_IF(std::length_error, count > max_size());

    clear();
    reserve(count);

    for (size_type i = 0; i < count; ++i) {
      new (GetElementPointer(i)) T(value);
    }

    size_ = count;
  }

  // Replaces the contents with copies of those in the range [first, last).
  template <class InputIt,
            class = internal::EnableIfLegacyInputIterator<InputIt>>
  constexpr void assign(InputIt first, InputIt last) {
    const size_type count = std::distance(first, last);

    TL_STATIC_VECTOR_THROW_IF(std::length_error, count > max_size());

    clear();
    reserve(count);

    for (size_type i = 0; first != last; ++first, ++i) {
      new (GetElementPointer(i)) T(*first);
    }

    size_ = count;
  }

  // Replaces the contents with the elements from the initializer list ilist.
  constexpr void assign(std::initializer_list<T> ilist) {
    assign(ilist.begin(), ilist.end());
  }

  // Element access
  // ==============

  // Returns a reference to the element at specified location pos, with bounds
  // checking.
  // If pos is not within the range of the container, an exception of type
  // std::out_of_range is thrown.
  constexpr auto at(const size_type pos) -> reference {
    TL_STATIC_VECTOR_THROW_IF(std::out_of_range, pos >= size());
    return data()[pos];
  }
  constexpr auto at(size_type pos) const -> const_reference {
    TL_STATIC_VECTOR_THROW_IF(std::out_of_range, pos >= size());
    return data()[pos];
  }

  // Returns a reference to the element at specified location pos.
  // No bounds checking is performed.
  constexpr auto operator[](const size_type pos) -> reference {
    return data()[pos];
  }
  constexpr auto operator[](const size_type pos) const -> const_reference {
    return data()[pos];
  }

  // Returns a reference to the first element in the container.
  // Calling front on an empty container is undefined.
  constexpr auto front() -> reference { return operator[](0); }
  constexpr auto front() const -> const_reference { return operator[](0); }

  // Returns a reference to the last element in the container.
  // Calling back on an empty container is undefined.
  constexpr auto back() -> reference { return operator[](size() - 1); }
  constexpr auto back() const -> const_reference {
    return operator[](size() - 1);
  }

  // Returns pointer to the underlying array serving as element storage.
  // The pointer is such that range [data(); data()+size()) is always a valid
  // range, even if the container is empty (data() is not dereferenceable in
  // that case).
  constexpr auto data() noexcept -> T* { return reinterpret_cast<T*>(data_); }
  constexpr auto data() const noexcept -> const T* {
    return reinterpret_cast<const T*>(data_);
  }

  // Iterators
  // =========

  // Returns an iterator to the first element of the vector.
  // begin() returns a mutable or constant iterator, depending on the constness
  // of *this.
  // cbegin() always returns a constant iterator. It is equivalent to
  // const_cast<const StaticVector&>(*this).begin().
  constexpr auto begin() noexcept -> iterator { return data(); }
  constexpr auto begin() const noexcept -> const_iterator { return data(); }
  constexpr auto cbegin() const noexcept -> const_iterator { return data(); }

  // Returns an iterator to the element following the last element of the
  // vector.
  constexpr auto end() noexcept -> iterator { return data() + size(); }
  constexpr auto end() const noexcept -> const_iterator {
    return data() + size();
  }
  constexpr auto cend() const noexcept -> const_iterator {
    return data() + size();
  }

  // Returns a reverse iterator to the first element of the reversed vector.
  // It corresponds to the last element of the non-reversed vector.
  constexpr auto rbegin() noexcept -> reverse_iterator {
    return reverse_iterator{end()};
  }
  constexpr auto rbegin() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator{cend()};
  }
  constexpr auto crbegin() const noexcept -> const_reverse_iterator {
    return const_reverse_iterator{cend()};
  }

  // Returns a reverse iterator to the element following the last element of
  // the reversed vector. It corresponds to the element preceding the first
  // element of the non-reversed vector.
  // This element acts as a placeholder, attempting to access it results in
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

  // Checks if the vector has no elements, i.e. whether size() == 0.
  [[nodiscard]] constexpr auto empty() const noexcept -> bool {
    return size() == 0;
  }

  // Returns the number of elements in the container.
  constexpr auto size() const noexcept -> size_type { return size_; }

  // Returns the maximum number of elements the container is able to hold.
  constexpr auto max_size() const noexcept -> size_type { return N; }

  // Increase the capacity of the vector (the total number of elements that the
  // vector can hold without requiring reallocation) to a value that's greater
  // or equal to new_cap.
  constexpr void reserve(const size_type new_cap) {
    TL_STATIC_VECTOR_THROW_IF(std::length_error, new_cap > max_size());
  }

  // Returns the number of elements that the container has currently allocated
  // space for.
  constexpr auto capacity() const noexcept -> size_type { return max_size(); }

  // Requests the removal of unused capacity.
  constexpr void shrink_to_fit() {}

  // Modifiers
  // =========

  // Erases all elements from the container. After this call, size() returns
  // zero.
  constexpr void clear() noexcept {
    const size_type count = size();
    for (size_type i = 0; i < count; ++i) {
      DestroyElement(GetElementPointer(i));
    }

    size_ = 0;
  }

  // Inserts value before pos.
  constexpr auto insert(const const_iterator pos, const T& value) -> iterator {
    TL_STATIC_VECTOR_THROW_IF(std::length_error, size() + 1 > max_size());

    if (pos == end()) {
      push_back(value);
      return std::prev(end());
    }

    iterator it = MoveElementsBack(pos, 1);
    new (&(*it)) T(value);

    return it;
  }
  constexpr auto insert(const const_iterator pos, T&& value) -> iterator {
    TL_STATIC_VECTOR_THROW_IF(std::length_error, size() + 1 > max_size());

    if (pos == end()) {
      emplace_back(std::move(value));
      return std::prev(end());
    }

    iterator it = MoveElementsBack(pos, 1);
    new (&(*it)) T(std::move(value));

    return it;
  }

  // Inserts count copies of the value before pos.
  constexpr auto insert(const const_iterator pos,
                        const size_type count,
                        const T& value) -> iterator {
    TL_STATIC_VECTOR_THROW_IF(std::length_error, size() + count > max_size());

    if (pos == end()) {
      for (size_type i = 0; i < count; ++i) {
        emplace_back(value);
      }
      return std::prev(end(), count);
    }

    iterator it = MoveElementsBack(pos, count);

    for (size_type i = 0; i < count; ++i) {
      new (&(*(it + i))) T(value);
    }

    return it;
  }

  // Inserts elements from range [first, last) before pos.
  template <class InputIt,
            class = internal::EnableIfLegacyInputIterator<InputIt>>
  constexpr auto insert(const const_iterator pos, InputIt first, InputIt last)
      -> iterator {
    const size_type count = std::distance(first, last);

    TL_STATIC_VECTOR_THROW_IF(std::length_error, size() + count > max_size());

    if (pos == end()) {
      for (; first != last; ++first) {
        emplace_back(*first);
      }
      return std::prev(end(), count);
    }

    iterator it = MoveElementsBack(pos, count);

    for (size_type i = 0; first != last; ++first, ++i) {
      new (&(*(it + i))) T(*first);
    }

    return it;
  }

  // Inserts elements from initializer list ilist before pos.
  constexpr auto insert(const const_iterator pos,
                        std::initializer_list<T> ilist) -> iterator {
    return insert(pos, ilist.begin(), ilist.end());
  }

  // Inserts a new element into the container directly before pos.
  template <class... Args>
  constexpr auto emplace(const const_iterator pos, Args&&... args) -> iterator {
    TL_STATIC_VECTOR_THROW_IF(std::length_error, size() + 1 > max_size());

    if (pos == end()) {
      emplace_back(std::forward<Args>(args)...);
      return std::prev(end());
    }

    iterator it = MoveElementsBack(pos, 1);
    new (&(*it)) T(std::forward<Args>(args)...);

    return it;
  }

  // Erases the specified elements from the container.
  // Returns iterator following the last removed element.
  //
  // If pos refers to the last element, then the end() iterator is returned.
  //
  // If last == end() prior to removal, then the updated end() iterator is
  // returned.
  //
  // If [first, last) is an empty range, then last is returned.
  constexpr auto erase(const const_iterator pos) -> iterator {
    return erase(pos, pos + 1);
  }
  constexpr auto erase(const const_iterator first, const const_iterator last)
      -> iterator {
    const size_type start_index = std::distance(cbegin(), first);
    const size_type count = std::distance(first, last);

    T* mem = data();

    // Destroy elements in the erase range.
    for (size_type i = 0; i < count; ++i) {
      DestroyElement(&mem[start_index + i]);
    }

    if (last != end()) {
      const size_type num_relocate = size() - start_index - count;

      T* dst = &mem[start_index];
      T* src = &mem[start_index + count];

      for (size_type i = 0; i < num_relocate; ++i, ++dst, ++src) {
        new (dst) T(std::move(*src));
        DestroyElement(src);
      }
    }

    size_ -= count;

    return begin() + start_index;
  }

  // Appends the given element value to the end of the container.
  constexpr void push_back(const T& value) {
    TL_STATIC_VECTOR_THROW_IF(std::length_error, size() + 1 > max_size());

    new (GetElementPointer(size_)) T(value);
    ++size_;
  }
  constexpr void push_back(T&& value) {
    TL_STATIC_VECTOR_THROW_IF(std::length_error, size() + 1 > max_size());

    new (GetElementPointer(size_)) T(std::move(value));
    ++size_;
  }

  // Appends a new element to the end of the container.
  template <class... Args>
  constexpr auto emplace_back(Args&&... args) -> reference {
    TL_STATIC_VECTOR_THROW_IF(std::length_error, size() + 1 > max_size());

    T* ptr = GetElementPointer(size_);
    new (ptr) T(std::forward<Args>(args)...);
    ++size_;

    return *ptr;
  }

  // Removes the last element of the container.
  // Calling pop_back on an empty container results in undefined behavior.
  constexpr void pop_back() {
    T* element = GetElementPointer(size_ - 1);

    DestroyElement(element);

    --size_;
  }

  // Resizes the container to contain count elements.
  // If the current size is greater than count, the container is reduced to its
  // first count elements.
  // If the current size is less than count, either the default-initialized or a
  // copies of value are appended.
  constexpr void resize(const size_type count) {
    TL_STATIC_VECTOR_THROW_IF(std::length_error, size() + count > max_size());

    if (size_ < count) {
      for (size_type i = 0; i < count - size_; ++i) {
        new (GetElementPointer(size_ + i)) T();
      }
    } else {
      for (size_type i = count; i < size_; ++i) {
        DestroyElement(GetElementPointer(i));
      }
    }

    size_ = count;
  }
  constexpr void resize(size_type count, const value_type& value) {
    TL_STATIC_VECTOR_THROW_IF(std::length_error, size() + count > max_size());

    if (size_ < count) {
      for (size_type i = 0; i < count - size_; ++i) {
        new (GetElementPointer(size_ + i)) T(value);
      }
    } else {
      for (size_type i = count; i < size_; ++i) {
        DestroyElement(GetElementPointer(i));
      }
    }

    size_ = count;
  }

  // Exchanges the contents of the container with those of other.
  //
  // Due to the nature of the storage performs per-element swap, using move
  // semantic and a temporary variable.
  constexpr void swap(StaticVector& other) {
    const size_type own_size = size();
    const size_type other_size = other.size();

    T* own_mem = data();
    T* other_mem = other.data();

    const size_type min_size = own_size < other_size ? own_size : other_size;
    for (size_type i = 0; i < min_size; ++i) {
      T tmp = std::move(own_mem[i]);
      own_mem[i] = std::move(other_mem[i]);
      other_mem[i] = std::move(tmp);
    }

    if (own_size > other_size) {
      for (size_type i = min_size; i < own_size; ++i) {
        new (&other_mem[i]) T(std::move(own_mem[i]));
        DestroyElement(&own_mem[i]);
      }
    } else {
      for (size_type i = min_size; i < other_size; ++i) {
        new (&own_mem[i]) T(std::move(other_mem[i]));
        DestroyElement(&other_mem[i]);
      }
    }

    std::swap(size_, other.size_);
  }

 private:
  // Mark the whole memory as uninitialized.
  //
  // Fills in memory with a gibberish pattern, giving address sanitizer and
  // developer chance to catch memory issues.
  constexpr void MarkMemoryUninitialized() noexcept {
    for (size_type i = 0; i < N; ++i) {
      MarkMemoryUninitialized(data() + i);
    }
  }

  // Mark memory which was used by the element as uninitialized.
  //
  // Fills in memory with a gibberish pattern, giving address sanitizer and
  // developer chance to catch memory issues.
  constexpr void MarkMemoryUninitialized(T* element) noexcept {
#if TL_STATIC_VECTOR_USE_MEMORY_DEBUG
    auto* mem = reinterpret_cast<uint8_t*>(element);
    for (size_type i = 0; i < sizeof(T); ++i) {
      mem[i] = 0xef;
    }
#else
    (void)element;
#endif
  }

  // Run the destructor of the element and mark its memory as uninitialized.
  constexpr void DestroyElement(T* element) {
    element->~T();
    MarkMemoryUninitialized(element);
  }

  // Reset the vector state when it has been moved somewhere else.
  constexpr void ResetOnMove() noexcept {
    MarkMemoryUninitialized();
    size_ = 0;
  }

  // Get pointer to an element memory with the given index.
  constexpr auto GetElementPointer(const size_type index) noexcept -> T* {
    return &data()[index];
  }
  constexpr auto GetElementPointer(const size_type index) const noexcept
      -> const T* {
    return &data()[index];
  }

  // Move the elements the given number of position to the back, starting from
  // the given position. This will increase the size of the vector.
  //
  // Returns iterator to the first position at which the new element can be
  // written to.
  //
  // The count elements starting from the return value is left in an
  // uninitialized memory state.
  auto MoveElementsBack(const const_iterator pos, const size_type count)
      -> iterator {
    const size_type index = pos - data();

    // The total number of elements to be relocated.
    const size_type num_relocate = size() - index;

    size_ += count;

    iterator dst_it = std::prev(end());
    iterator src_it = std::prev(dst_it, count);

    for (size_type i = 0; i < num_relocate; ++i) {
      new (&(*dst_it)) T(std::move(*src_it));

      DestroyElement(&(*src_it));

      dst_it = std::prev(dst_it);
      src_it = std::prev(src_it);
    }

    return data() + index;
  }

  uint8_t data_[sizeof(T) * N];  // NOLINT(modernize-avoid-c-arrays)
  size_type size_{0};
};

////////////////////////////////////////////////////////////////////////////////
// Non-member functions.

// Checks if the contents of lhs and rhs are equal, that is, they have the same
// number of elements and each element in lhs compares equal with the element in
// rhs at the same position.
template <class T, std::size_t N, std::size_t M>
constexpr auto operator==(const StaticVector<T, N>& lhs,
                          const StaticVector<T, M>& rhs) -> bool {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (typename StaticVector<T, N>::size_type i = 0; i < lhs.size(); ++i) {
    if (lhs[i] != rhs[i]) {
      return false;
    }
  }

  return true;
}
template <class T, std::size_t N, std::size_t M>
constexpr auto operator!=(const StaticVector<T, N>& lhs,
                          const StaticVector<T, M>& rhs) -> bool {
  return !(lhs == rhs);
}

// Compares the contents of lhs and rhs lexicographically.
//
// NOTE: Use the pre-C++20 definitions since the C++20 is relying on the
// lexicographical_compare_three_way which is not available in all compiles
// at the moment this code is written.
template <class T, std::size_t N, std::size_t M>
constexpr auto operator<(const StaticVector<T, N>& lhs,
                         const StaticVector<T, M>& rhs) {
  return std::lexicographical_compare(
      lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}
template <class T, std::size_t N, std::size_t M>
constexpr auto operator>(const StaticVector<T, N>& lhs,
                         const StaticVector<T, M>& rhs) {
  return rhs < lhs;
}
template <class T, std::size_t N, std::size_t M>
constexpr auto operator<=(const StaticVector<T, N>& lhs,
                          const StaticVector<T, M>& rhs) {
  return !(lhs > rhs);
}
template <class T, std::size_t N, std::size_t M>
constexpr auto operator>=(const StaticVector<T, N>& lhs,
                          const StaticVector<T, M>& rhs) {
  return !(lhs < rhs);
}

// Specializes the swap() algorithm for StaticVector.
// Swaps the contents of lhs and rhs. Equivalent to `lhs.swap(rhs)`.
template <class T, std::size_t N>
constexpr void swap(StaticVector<T, N>& lhs, StaticVector<T, N>& rhs) noexcept {
  lhs.swap(rhs);
}

// Erases all elements that compare equal to value from the container.
// Returns the number of erased elements.
template <class T, std::size_t N, class U>
constexpr auto erase(StaticVector<T, N>& c, const U& value) ->
    typename StaticVector<T, N>::size_type {
  auto* it = std::remove(c.begin(), c.end(), value);
  auto r = std::distance(it, c.end());
  c.erase(it, c.end());
  return r;
}

// Erases all elements that satisfy the predicate pred from the container
// Returns the number of erased elements.
template <class T, std::size_t N, class Pred>
constexpr auto erase_if(StaticVector<T, N>& c, Pred pred) ->
    typename StaticVector<T, N>::size_type {
  auto* it = std::remove_if(c.begin(), c.end(), pred);
  auto r = std::distance(it, c.end());
  c.erase(it, c.end());
  return r;
}

// NOLINTEND(readability-identifier-naming)

}  // namespace TL_STATIC_VECTOR_VERSION_NAMESPACE
}  // namespace TL_STATIC_VECTOR_NAMESPACE

#undef TL_STATIC_VECTOR_VERSION_MAJOR
#undef TL_STATIC_VECTOR_VERSION_MINOR
#undef TL_STATIC_VECTOR_VERSION_REVISION

#undef TL_STATIC_VECTOR_NAMESPACE

#undef TL_STATIC_VECTOR_VERSION_NAMESPACE_CONCAT_HELPER
#undef TL_STATIC_VECTOR_VERSION_NAMESPACE_CONCAT
#undef TL_STATIC_VECTOR_VERSION_NAMESPACE

#undef TL_STATIC_VECTOR_THROW_IF

#undef TL_STATIC_VECTOR_NO_MEMORY_DEBUG
#undef TL_STATIC_VECTOR_USE_MEMORY_DEBUG
