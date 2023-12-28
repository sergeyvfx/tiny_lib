// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

// An optional contained value with an error information associated with it.
//
// The error is a required part of the Result in the case the value is not
// known. This error will provide details about reasoning why the result could
// not be calculated.
//
// It is possible to have result which has both value and error associated with
// it. In this case the result is considered to be ill-calculated, but it is
// allowed to access the partially calculated value.
//
// The result is considered to be successfully calculated if and only if there
// is no error associated with it.
//
// A common use case for the Result is the return value of a function which
// might fail and wants to communicate details about the failure mode.
//
// Example:
//
//   enum class Error {
//     GENERIC_ERROR,
//     OUT_OF_MEMORY,
//     ACCESS_DENIED,
//   };
//
//   ...
//
//   Result<Foo, Error> result = Calculate();
//   if (result.Ok()) {
//     result->DoSomethingInteresting();
//   } else {
//     std::cerr << "Calculation failed: " << result.GetError() << std::endl;
//   }
//
// It is possible to use non-enumerator error code like boolean. The Result
// does not add any semantic to the value, so both true and false error codes
// in this case will be considered a non-OK result.
//
// NOTE: The ValueType and ErrorType should not be implicitly convertible
// between each-other.
//
//
// Version history
// ===============
//
//   0.0.1-alpha    (28 Dec 2023)    First public release.

#pragma once

#include <cassert>
#include <optional>
#include <ostream>
#include <type_traits>

// Semantic version of the tl_result library.
#define TL_RESULT_VERSION_MAJOR 0
#define TL_RESULT_VERSION_MINOR 0
#define TL_RESULT_VERSION_REVISION 1

// Namespace of the module.
// The outer name spaces which surrounds the ABI-version namespace.
#ifndef TL_RESULT_NAMESPACE
#  define TL_RESULT_NAMESPACE tiny_lib::result
#endif

// Helpers for TL_RESULT_VERSION_NAMESPACE.
//
// Typical extra indirection for such conversion to allow macro to be expanded
// before it is converted to string.
#define TL_RESULT_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)               \
  v_##id1##_##id2##_##id3
#define TL_RESULT_VERSION_NAMESPACE_CONCAT(id1, id2, id3)                      \
  TL_RESULT_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)

// Constructs identifier suitable for namespace denoting the current library
// version.
//
// For example: TL_RESULT_VERSION_NAMESPACE -> v_0_1_9
#define TL_RESULT_VERSION_NAMESPACE                                            \
  TL_RESULT_VERSION_NAMESPACE_CONCAT(TL_RESULT_VERSION_MAJOR,                  \
                                     TL_RESULT_VERSION_MINOR,                  \
                                     TL_RESULT_VERSION_REVISION)

// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace TL_RESULT_NAMESPACE {
inline namespace TL_RESULT_VERSION_NAMESPACE {

// The result shall not be silently ignored.
template <class ValueType, class ErrorType>
class [[nodiscard]] Result;

namespace result_internal {

template <class T>
using IsInPlaceType = std::is_same<std::remove_cvref_t<T>, std::in_place_t>;

template <class T>
using IsNotInPlaceType = std::negation<IsInPlaceType<T>>;

template <class ValueType, class OtherValueType, class ErrorType>
using IsDirectInitializable = std::conjunction<
    std::is_constructible<ValueType, OtherValueType&&>,
    IsNotInPlaceType<OtherValueType>,
    std::negation<std::is_same<std::remove_cvref_t<OtherValueType>,
                               Result<ValueType, ErrorType>>>>;

template <class ValueType, class OtherValueType, class ErrorType>
using IsDirectInitializableImplicit = std::conjunction<
    IsDirectInitializable<ValueType, OtherValueType, ErrorType>,
    std::is_convertible<OtherValueType&&, ValueType>>;

template <class ValueType, class OtherValueType, class ErrorType>
using IsDirectInitializableExplicit = std::conjunction<
    IsDirectInitializable<ValueType, OtherValueType, ErrorType>,
    std::negation<std::is_convertible<OtherValueType&&, ValueType>>>;

}  // namespace result_internal

template <class ValueType, class ErrorType>
class Result {
 public:
  // Constructs an object that does not contain a value.
  constexpr Result(const ErrorType error) : error_(error) {}

  // Copy and move constructors.
  // Follows semantic of the std::optional.
  constexpr Result(const Result& other) = default;
  constexpr Result(Result&& other) noexcept = default;

  // TODO(sergey): Implement all constructors from the std::optional.

  // Constructs a Result object that contains a value, initialized as if
  // direct-initializing (but not direct-list-initializing) an object of type
  // ValueType with the expression std::forward<OtherValueType>(value).

  template <class OtherValueType = ValueType,
            std::enable_if_t<result_internal::IsDirectInitializableImplicit<
                                 ValueType,
                                 OtherValueType,
                                 ErrorType>::value,
                             bool> = true>
  constexpr Result(OtherValueType&& value)
      : value_(std::forward<OtherValueType>(value)) {}

  template <class OtherValueType = ValueType,
            std::enable_if_t<result_internal::IsDirectInitializableExplicit<
                                 ValueType,
                                 OtherValueType,
                                 ErrorType>::value,
                             bool> = true>
  explicit constexpr Result(OtherValueType&& value)
      : value_(std::forward<OtherValueType>(value)) {}

  // Constructs a Result object that contains a value, initialized as if
  // direct-initializing (but not direct-list-initializing) an object of type
  // ValueType with the expression std::forward<OtherValueType>(value), and an
  // error.
  template <
      class OtherValueType = ValueType,
      std::enable_if_t<result_internal::IsDirectInitializable<ValueType,
                                                              OtherValueType,
                                                              ErrorType>::value,
                       bool> = true>
  constexpr Result(OtherValueType&& value, const ErrorType error)
      : value_(std::forward<OtherValueType>(value)), error_(error) {}

  ~Result() = default;

  constexpr auto operator=(const Result& other) -> Result& = default;
  constexpr auto operator=(Result&& other) noexcept -> Result& = default;

  constexpr auto operator==(const Result& other) const -> bool {
    return value_ == other.value_ && error_ == other.error_;
  }
  constexpr auto operator!=(const Result& other) const -> bool {
    return !(*this == other);
  }

  // TODO(sergey): Implement assignment operator.

  // Checks whether *this contains a value.
  // When the value is present an error is considered to be absent.
  constexpr auto Ok() const noexcept -> bool { return !error_.has_value(); }

  // When the result is not ok returns the error code.
  // When is called for result which is ok throws a std::bad_optional_access
  // exception.
  constexpr auto GetError() const -> const ErrorType& { return error_.value(); }

  // Checks whether the result contains a value.
  constexpr auto HasValue() const noexcept -> bool {
    return value_.has_value();
  }

  // If *this contains a value, returns a reference to the contained value.
  // Otherwise, throws a std::bad_optional_access exception.
  constexpr auto GetValue() & -> ValueType& { return value_.value(); }
  constexpr auto GetValue() const& -> const ValueType& {
    return value_.value();
  }
  constexpr auto GetValue() && -> ValueType&& { return value_.value(); }
  constexpr auto GetValue() const&& -> const ValueType&& {
    return value_.value();
  }

  // Accesses the contained value.
  //
  // Leads to an undefined behavior when the result is not ok.
  constexpr auto operator->() const noexcept -> const ValueType* {
    assert(HasValue());
    return &(*value_);
  }
  constexpr auto operator->() noexcept -> ValueType* {
    assert(HasValue());
    return &(*value_);
  }

  constexpr auto operator*() const& noexcept -> const ValueType& {
    return *value_;
  }
  constexpr auto operator*() & noexcept -> ValueType& { return *value_; }
  constexpr auto operator*() const&& noexcept -> const ValueType&& {
    return *value_;
  }
  constexpr auto operator*() && noexcept -> ValueType&& { return *value_; }

 private:
  std::optional<ValueType> value_;
  std::optional<ErrorType> error_;
};

namespace result_internal {

// Printer of various objects to a stream in the most human readable manner
// possible.
//
// The printing rules are happening in the following order;
//
//  - If an object implements operator<< to a stream this operator is used.
//
//  - Enumeration values are printed as integer values corresponding to the
//    internal value of the enumerator value.
//
//  - When nothing from above fits then the object is printed as a memory
//    dump. When object os too large then only head and tail of its memory is
//    printed.
//
// Example:
//
//   class MyClass {
//     int foo;
//     float bar;
//   };
//
//   MyClass object;
//   ValuePrinter::PrintTo(object, std::cout);

namespace value_printer_internal {

static_assert(sizeof(unsigned char) == 1);

template <class T>
struct HasPutToStreamOperatorImpl {
  template <class U>
  static auto Test(U*)
      -> decltype(std::declval<std::ostream>() << std::declval<U>());

  template <class U>
  static auto Test(...) -> std::false_type;

  // The naming follows the STL convention.
  // NOLINTNEXTLINE(readability-identifier-naming)
  using type = typename std::negation<
      std::is_same<std::false_type, decltype(Test<T>(nullptr))>>::type;
};

// If T can be put to a `std::ostream stream`, provides the member constant
// value equal to `true`.
// Otherwise value is `false`.
template <class T>
struct HasPutToStreamOperator : HasPutToStreamOperatorImpl<T>::type {};

// Print a sub-span of the memory as human readable hexadecimal values.
//
// Use old-style indices instead of std::span to minimize dependencies of this
// file.
inline void PrintObjectBytesSegmentTo(const unsigned char* data,
                                      const size_t start_byte,
                                      const size_t num_bytes,
                                      std::ostream& os) {
  // Digits in the hexidecimal base.
  //
  // clang-format off
  //
  // NOTE: Not using modern std::array to minimize amount of headers pulled in.
  // NOLINTNEXTLINE(modernize-avoid-c-arrays)
  constexpr char kHexDigits[] = {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
      'A', 'B', 'C', 'D', 'E', 'F',
  };
  // clang-format on

  // Buffer for hexidecimal conversion.
  //
  // NOTE: Not using modern std::array to minimize amount of headers pulled in.
  char hex[3] = "";  // NOLINT(modernize-avoid-c-arrays)

  for (int i = 0; i < num_bytes; ++i) {
    const size_t byte_index = start_byte + i;

    // Convert byte to hex.
    const unsigned char byte = data[byte_index];
    hex[0] = kHexDigits[byte / 16];
    hex[1] = kHexDigits[byte % 16];

    if (i != 0) {
      if ((byte_index % 2) == 0) {
        os << ' ';
      } else {
        os << '-';
      }
    }

    os << hex;
  }
}

// Print object as a human-readable memory dump.
inline void PrintObjectBytesTo(const unsigned char* data,
                               const size_t num_bytes,
                               std::ostream& os) {
  os << num_bytes << "-byte object <";

  // Maximum size of an object in bytes which printed as a whole continuous
  // memory dump. When this size is exceeded only head and tail of the object
  // memory is printed.
  constexpr size_t kMaxFullObjectSizeInBytes = 132;

  // Size of head and tail in bytes when the object memory is dumped partially.
  constexpr size_t kChunkSizeInBytes = 64;

  if (num_bytes < kMaxFullObjectSizeInBytes) {
    PrintObjectBytesSegmentTo(data, 0, num_bytes, os);
  } else {
    PrintObjectBytesSegmentTo(data, 0, kChunkSizeInBytes, os);

    os << " ... ";

    // Round tail start to the 2-byte boundary.
    const size_t tail_start = (num_bytes - kChunkSizeInBytes + 1) / 2 * 2;

    PrintObjectBytesSegmentTo(data, tail_start, num_bytes - tail_start, os);
  }

  os << ">";
}

}  // namespace value_printer_internal

class ValuePrinter {
 public:
  template <class T>
  static void PrintTo(const T& value, std::ostream& os) {
    if constexpr (value_printer_internal::HasPutToStreamOperator<T>::value) {
      os << value;
      return;
    }

    if constexpr (std::is_enum_v<T>) {
      os << static_cast<typename std::underlying_type<T>::type>(value);
      return;
    }

    value_printer_internal::PrintObjectBytesTo(
        reinterpret_cast<const unsigned char*>(&value), sizeof(T), os);
  }
};

}  // namespace result_internal

template <class ValueType, class ErrorType>
inline auto operator<<(std::ostream& os,
                       const Result<ValueType, ErrorType>& result)
    -> std::ostream& {
  if (result.HasValue()) {
    os << "value:";
    result_internal::ValuePrinter::PrintTo(result.GetValue(), os);
  }

  if (!result.Ok()) {
    os << "error:";
    result_internal::ValuePrinter::PrintTo(result.GetError(), os);
  }

  return os;
}

}  // namespace TL_RESULT_VERSION_NAMESPACE
}  // namespace TL_RESULT_NAMESPACE

#undef TL_RESULT_VERSION_MAJOR
#undef TL_RESULT_VERSION_MINOR
#undef TL_RESULT_VERSION_REVISION

#undef TL_RESULT_NAMESPACE

#undef TL_RESULT_VERSION_NAMESPACE_CONCAT_HELPER
#undef TL_RESULT_VERSION_NAMESPACE_CONCAT
#undef TL_RESULT_VERSION_NAMESPACE
