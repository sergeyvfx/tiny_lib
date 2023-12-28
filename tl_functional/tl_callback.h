// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

// Simple implementation of a callback with an attachable listeners.
//
// NOTE: Experimental functionality. The API might get changed, or even removed
//       if the library turns out not to be very reusable.
//
// The callback itself is a function-like object which can be called with a list
// of arguments, which are forwarded to an invocation calls of all registered
// listeners.
//
// The implementation is thread safe: adding, removing, and invoking the
// callback could happen from a concurrent threads.
//
//
// Limitations
// ===========
//
//  - Uses std::function for function pointer and capture. The behavior is
//    STL implementation specific, might require allocations.
//
//  - Uses std::list<> for container, hence requires heap allocation.
//
//  - Uses std::mutex for thread safety. It could be unavailable on
//    microcontrollers.
//
//
// Version history
// ===============
//
//   0.0.1-alpha    (28 Dec 2023)    First public release.

#pragma once

#include <functional>
#include <list>
#include <mutex>

// Semantic version of the tl_callback library.
#define TL_CALLBACK_VERSION_MAJOR 0
#define TL_CALLBACK_VERSION_MINOR 0
#define TL_CALLBACK_VERSION_REVISION 1

// Namespace of the module.
// The outer name spaces which surrounds the ABI-version namespace.
#ifndef TL_CALLBACK_NAMESPACE
#  define TL_CALLBACK_NAMESPACE tiny_lib::callback
#endif

// Helpers for TL_CALLBACK_VERSION_NAMESPACE.
//
// Typical extra indirection for such conversion to allow macro to be expanded
// before it is converted to string.
#define TL_CALLBACK_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)             \
  v_##id1##_##id2##_##id3
#define TL_CALLBACK_VERSION_NAMESPACE_CONCAT(id1, id2, id3)                    \
  TL_CALLBACK_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)

// Constructs identifier suitable for namespace denoting the current library
// version.
//
// For example: TL_CALLBACK_VERSION_NAMESPACE -> v_0_1_9
#define TL_CALLBACK_VERSION_NAMESPACE                                          \
  TL_CALLBACK_VERSION_NAMESPACE_CONCAT(TL_CALLBACK_VERSION_MAJOR,              \
                                       TL_CALLBACK_VERSION_MINOR,              \
                                       TL_CALLBACK_VERSION_REVISION)

// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace TL_CALLBACK_NAMESPACE {
inline namespace TL_CALLBACK_VERSION_NAMESPACE {

template <class>
class Callback;

template <class R, class... Args>
class Callback<R(Args...)> {
  // Internal function type representing listener of this callback.
  using Function = std::function<R(Args...)>;

  // Storage for registered listeners.
  //
  // Use list to allow having an easy way to use list iterator as an identifier
  // od the listeners without them becoming invalid when adding or removing
  // listeners.
  //
  // TODO(sergey): Consider adding possibility to use staticly sized container,
  // to help adopting the code for microcontrollers.
  using Listeners = std::list<Function>;

  // Helper class to wrap implementation details of the ID.
  template <class T>
  struct Wrap {
   public:
    Wrap() = default;

   private:
    friend class Callback;

    Wrap(const T& value) : value_(value) {}

    constexpr auto Get() const -> const T& { return value_; }

    T value_;
  };

 public:
  using Listener = Function;
  using ID = Wrap<typename Listeners::const_iterator>;

  // Add listener to the callback.
  //
  // Returns the identifier of the new listener. This ID can be used to remove
  // the listener from this callback.
  auto AddListener(Listener listener) -> ID {
    std::unique_lock lock(mutex_);

    listeners_.emplace_back(std::move(listener));
    return {std::prev(listeners_.end())};
  }

  // Remove listener with the given ID.
  // Invalidates the id. Calling with an invalid ID is undefined.
  auto RemoveListener(const ID id) {
    std::unique_lock lock(mutex_);

    listeners_.erase(id.Get());
  }

  // Remove all listeners.
  auto RemoveAllListeners() { listeners_.clear(); }

  // Execute the callback.
  //
  // Will invoke all currently registered listeners with the forwarded
  // arguments. The return value of the listeners is ignored.
  //
  // The listeners are invoked in their order or registration.
  void operator()(Args... args) const {
    std::unique_lock lock(mutex_);

    for (auto& listener : listeners_) {
      std::invoke(listener, std::forward<Args>(args)...);
    }
  }

 private:
  // Mutable to allow use from const methods where thread-safety is needed.
  mutable std::mutex mutex_;

  // Storage of registered listeners.
  Listeners listeners_;
};

}  // namespace TL_CALLBACK_VERSION_NAMESPACE
}  // namespace TL_CALLBACK_NAMESPACE

#undef TL_CALLBACK_VERSION_MAJOR
#undef TL_CALLBACK_VERSION_MINOR
#undef TL_CALLBACK_VERSION_REVISION

#undef TL_CALLBACK_NAMESPACE

#undef TL_CALLBACK_VERSION_NAMESPACE_CONCAT_HELPER
#undef TL_CALLBACK_VERSION_NAMESPACE_CONCAT
#undef TL_CALLBACK_VERSION_NAMESPACE
