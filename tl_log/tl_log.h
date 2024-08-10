// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

// Building blocks for logging which happens to a application-dependent output.
//
// This logging implementation allows to easily support logging to a
// non-standard outut devices like UART modules of microcontrollers.
//
// There are two ways to perform interact with the logging API: the ostream and
// printf() style.
//
// The ostream style of API is implemented via subclassing the Functions class
// and providing implementation for a number of low-level application-specific
// functions like writing to an output. It is possible to put any object which
// implements operator<<(std::ostream&) method.
//
// NOTE: The downside of the ostream style API is that with typical STL it will
// pull a big chunk of locale related code, which is prone for not fitting into
// microcontrollers' flash.
//
// The printf() style of API is very similar to printf() itself: it just has an
// extra severity parameter which will denote where the logging happens.
// Formatting happens using std::vsnprintf().
//
// NOTE: The limitation of the printf() style of API is that the message is
// truncated by the size of the allocated buffer.
//
//
// Example
// =======
//
//   #define LOG_INFO
//       tiny_lib::log::Message(tiny_lib::log::Severity::kInfo).GetStream()
//   #define LOG_FORMAT_INFO(...)
//       tiny_lib::log::LogFormatted(
//           tiny_lib::log::Severity::kInfo, __VA_ARGS__)
//
//   class MyFunctions : public tiny_lib::log::Functions {
//    public:
//      static auto Get() -> Functions& {
//        static MyFunctions functions;
//        return functions;
//      }
//     ...
//   };
//
//   tiny_lib::log::Context::Get().SetFunctions(Functions::Get());
//
//   LOG_INFO << "Hello, World!";
//   LOG_FORMAT_INFO("Hello, %s!", "World");
//
//
// Limitations
// ===========
//
// - Logging from multiple threads needs some solution to avoid interleaved
//   characters in the output.
//
// - Using stream-based logging API might pull heavy on FLASH std::locale
//   functionality.
//
// - When using printf() style of API the messages are truncated by the size of
//   the allocated size.
//
//
// Version history
// ===============
//
//   0.0.2-alpha    (10 Aug 2024)    Fix infinite recursion in stream logger
//                                   when allocated work buffer is empty.
//   0.0.1-alpha    (28 Dec 2023)    First public release.

#pragma once

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ostream>
#include <streambuf>

// Semantic version of the tl_log library.
#define TL_LOG_VERSION_MAJOR 0
#define TL_LOG_VERSION_MINOR 0
#define TL_LOG_VERSION_REVISION 2

// Namespace of the module.
// The outer name spaces which surrounds the ABI-version namespace.
#ifndef TL_LOG_NAMESPACE
#  define TL_LOG_NAMESPACE tiny_lib::log
#endif

// Helpers for TL_LOG_VERSION_NAMESPACE.
//
// Typical extra indirection for such conversion to allow macro to be expanded
// before it is converted to string.
#define TL_LOG_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)                  \
  v_##id1##_##id2##_##id3
#define TL_LOG_VERSION_NAMESPACE_CONCAT(id1, id2, id3)                         \
  TL_LOG_VERSION_NAMESPACE_CONCAT_HELPER(id1, id2, id3)

// Constructs identifier suitable for namespace denoting the current library
// version.
//
// For example: TL_LOG_VERSION_NAMESPACE -> v_0_1_9
#define TL_LOG_VERSION_NAMESPACE                                               \
  TL_LOG_VERSION_NAMESPACE_CONCAT(                                             \
      TL_LOG_VERSION_MAJOR, TL_LOG_VERSION_MINOR, TL_LOG_VERSION_REVISION)

// The logger includes default implementation which uses stderr as an output.
//
// This simplifies integration of the library into the new projects because the
// library will give meaningful guidance, as well as provide an example
// implementation of the applications-specific functionality,
//
// However, on some platforms it might be more desirable to strip this
// implementation away in order to save code segment size and RAM. In this
// case the application is to define TL_LOG_NO_DEFAULT_IMPLEMENTATION before
// including this header.
#if !defined(TL_LOG_NO_DEFAULT_IMPLEMENTATION)
#  include <cstdio>
#endif

// A hint for the compiler that some piece of code was not meant to be executed
// and that there is no recovery from that code path. For example, if the fatal
// error sid sink actually return and now the code is about to return from a
// function which was not meant to be returned from.
// This also allows to silence strict compiler warning which could be generated
// even if the code is properly structured.
#if !defined(TL_LOG_UNREACHABLE_ABORT)
#  define TL_LOG_UNREACHABLE_ABORT() abort()
#endif

// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace TL_LOG_NAMESPACE {
inline namespace TL_LOG_VERSION_NAMESPACE {

// Severity of logging message.
enum class Severity {
  // Generic event indicating state change or guiding a debugging.
  kInfo,

  // Undesired but recoverable event which might indicate a problem.
  kWarning,

  // Undesired but recoverable event which does indicate a problem.
  // Typically errors indicate bugs and hence are to be submitted as a report
  // and acted upon.
  kError,

  // Undesired and unrecoverable event.
  kFatal,
};

// Implementation of low-level application-specific functions used by the
// logger.
class Functions {
 public:
  Functions() = default;
  virtual ~Functions() = default;

  // Write the message to the logging output.
  //
  // Note that the message is not guaranteed to be null-terminated.
  //
  // For example in the simplest case the implementation will print the message
  // to the stdout.
  virtual void Write(Severity severity, const char* message, size_t length) = 0;

  // Flush the current output buffer, ensuring that the message is fully written
  // to the logging output.
  virtual void Flush(Severity severity) = 0;

  // Fail the application execution on a fatal error.
  // It is called after a message of a fatal failure has been written.
  //
  // This function must not return.
  [[noreturn]] virtual void Fail() = 0;

  // Allocate put buffer for the message logging output.
  //
  // Characters of a message will be accumulated to the buffer of this size
  // prior of being written. The application specific implementation can decide
  // how big of the buffer it wants to use. Bigger buffer means less overhead
  // spent in the write function calls, but also means higher memory usage.
  //
  // Note that there could be multiple nested message printers (happens when
  // a function output value is logged and the function itself is using logging,
  // for example). The allocator needs to be aware of this and ensure that the
  // buffer memory is never re-used until it was freed.
  virtual auto AllocatePrintBuffer(size_t& buffer_size) -> char* = 0;

  // Free buffer allocated for a put area of a message printer.
  //
  // The buffer size matches the size provided by the allocator.
  virtual void FreePrintBuffer(char* buffer, size_t buffer_size) = 0;
};

#if !defined(TL_LOG_NO_DEFAULT_IMPLEMENTATION)
namespace internal {

class DefaultFunctions : public Functions {
 public:
  static auto Get() -> Functions& {
    static DefaultFunctions functions;
    return functions;
  }

  void Write(Severity /*severity*/,
             const char* message,
             const size_t length) override {
    static bool already_warned_before_init = false;
    if (!already_warned_before_init) {
      fprintf(
          stderr,
          "WARNING: Logging before providing application specific functions is "
          "written to STDERR\n");
      already_warned_before_init = true;
    }

    // Print in chunks maximum of this size.
    // This is because * format only works with int type.
    constexpr size_t kMaxNumCharsPerPrint = size_t(10) * 1024 * 1024;

    const char* ptr = message;
    size_t num_chars_printed = 0;
    while (num_chars_printed < length) {
      size_t num_chars_to_print = length - num_chars_printed;
      if (num_chars_to_print > kMaxNumCharsPerPrint) {
        num_chars_to_print = kMaxNumCharsPerPrint;
      }

      fprintf(stderr, "%.*s", int(num_chars_to_print), ptr);

      ptr += num_chars_to_print;
      num_chars_printed += num_chars_to_print;
    }
  }

  void Flush(Severity /*severity*/) override { fflush(stderr); }

  [[noreturn]] void Fail() override { std::abort(); }

  auto AllocatePrintBuffer(size_t& buffer_size) -> char* override {
    constexpr size_t kBufferSize = 1024;
    buffer_size = kBufferSize;
    return new char[kBufferSize];
  }

  void FreePrintBuffer(char* buffer, const size_t /*buffer_size*/) override {
    delete[] buffer;
  }
};

}  // namespace internal
#endif

// Logging context which defines behavior of all messages which are printed.
class Context {
 public:
  // Get the global context instance.
  static auto Get() -> Context& {
    static Context context;
    return context;
  }

  // Reset the context to its default state.
  // It will no longer be referencing any application specific objects.
  inline void Reset() {
#if !defined(TL_LOG_NO_DEFAULT_IMPLEMENTATION)
    SetFunctions(internal::DefaultFunctions::Get());
#else
    functions_ = nullptr;
#endif
  }

  // Set the application specific functions implementations used by this
  // context.
  //
  // Note that the context references the functions implementations, which means
  // that the functions object must be valid when logging happens.
  void SetFunctions(Functions& functions) { functions_ = &functions; }

  // Get currently configured functions implementation.
  auto GetFunctions() const -> Functions* { return functions_; }

  // Write the message to the logging output.
  //
  // Note that the message is not guaranteed to be null-terminated.
  inline void Write(const Severity severity,
                    const char* message,
                    const size_t length) {
    assert(functions_);
    functions_->Write(severity, message, length);
  }

  // Flush the current output buffer, ensuring that the message is fully written
  // to the logging output.
  inline void Flush(const Severity severity) {
    assert(functions_);
    functions_->Flush(severity);
  }

  // Fail the application execution on a fatal error.
  [[noreturn]] inline void Fail() {
    assert(functions_);
    functions_->Fail();
    TL_LOG_UNREACHABLE_ABORT();
  }

  // Allocate put buffer for the message logging output.
  [[nodiscard]] inline auto AllocatePrintBuffer(size_t& buffer_size) -> char* {
    assert(functions_);
    return functions_->AllocatePrintBuffer(buffer_size);
  }

  // Free buffer allocated for a put area of a message printer..
  inline void FreePrintBuffer(char* buffer, size_t buffer_size) {
    return functions_->FreePrintBuffer(buffer, buffer_size);
  }

 private:
  Context() { Reset(); }

  // Application specific low-level functions implementations.
  Functions* functions_{nullptr};
};

namespace internal {

// Stream buffer implementation which buffers output characters to a buffer and
// sends them to the logging output when either the buffer overflows or when the
// stream buffer object is being destroyed.
class StreamBuffer : public std::streambuf {
 public:
  explicit StreamBuffer(const Severity severity) : severity_(severity) {
    buffer_ = Context::Get().AllocatePrintBuffer(buffer_size_);

    setp(buffer_, buffer_ + buffer_size_);
  }

  inline auto overflow(const int ch) -> int override {
    if (buffer_size_ == 0) {
      // Silently consume the character.
      // The buffer was explicitly allocated to 0 size, likely for the
      // performance reasons to disable logging completely.
      return traits_type::to_int_type(ch);
    }

    has_overflow_ = true;

    // Write the current buffer to the output.
    WriteToOutput();

    // The buffer is empty now. Reposition the put pointer to the beginning of
    // the put area.
    setp(pbase(), epptr());

    // Put new character to the put area.
    return sputc(char(ch));
  }

  // Flush the stream after the message was fully put to stream.
  // Takes care of ensuring the new line at the end of the message, and makes
  // sure the message is fully written to the output.
  inline void Flush() {
    const bool has_non_written_buffer = pptr() != pbase();
    const bool has_output = has_overflow_ || has_non_written_buffer;
    if (!has_output) {
      return;
    }

    // Ensure that the message contains new line at the end.

    bool need_newline = false;
    if (has_non_written_buffer) {
      need_newline = *(pptr() - 1) != '\n';
    } else {
      need_newline = !is_last_written_newline_;
    }

    if (need_newline) {
      sputc('\n');
    }

    // Write the possibly updated buffer to the output.
    WriteToOutput();

    // Make sure the message is fully printed to the output.
    Context::Get().Flush(severity_);

    // The buffer is fully flushed.
    // Reset its state to the fully empty state.
    setp(pbase(), epptr());
    has_overflow_ = false;
  }

  inline auto GetSeverity() const -> Severity { return severity_; }

  // Disable copy and move semantic.
  //
  // Those are not trivial and not used, so implementing those does not solve
  // any practical issue, only unnecessarily extends the code base.
  StreamBuffer(const StreamBuffer& other) = delete;
  StreamBuffer(StreamBuffer&& other) noexcept = delete;
  auto operator=(const StreamBuffer& other) -> StreamBuffer& = delete;
  auto operator=(StreamBuffer&& other) -> StreamBuffer& = delete;

 private:
  // Hide the swap method as it is not implemented
  using std::streambuf::swap;

  // Write current buffer to the logging output.
  void WriteToOutput() {
    const size_t length = pptr() - pbase();
    if (length == 0) {
      return;
    }

    is_last_written_newline_ = buffer_[length - 1] == '\n';
    Context::Get().Write(severity_, buffer_, length);
  }

  Severity severity_;

  // The put buffer has been overflown and hence some characters were written
  // from the buffer to the logger output.
  bool has_overflow_{false};

  // True when the last character sent to Write() is the newline.
  bool is_last_written_newline_{false};

  // Put buffer and its size.
  char* buffer_{nullptr};
  size_t buffer_size_{0};
};

// Implementation of an output stream which uses StreamBuffer for output.
class BufferredStream : public std::ostream {
 public:
  explicit BufferredStream(const Severity severity)
      : std::ostream(nullptr), stream_buffer_(severity) {
    rdbuf(&stream_buffer_);
  }

  // Flush the stream after the message was fully put to stream.
  // Takes care of ensuring the new line at the end of the message, and makes
  // sure the message is fully written to the output.
  inline void Flush() { stream_buffer_.Flush(); }

  inline auto GetSeverity() const -> Severity {
    return stream_buffer_.GetSeverity();
  }

 private:
  StreamBuffer stream_buffer_;
};

// Log message as-is, handling severity and new lines as needed:
// - New line at the end of the text is ensured.
// - Logging happens to an underlying output corresponding to the severity
//   (which is decided by the provided logging functions).
// If the severity is Fatal only the logging part is handled here.
inline void LogMessageUnformatted(const Severity severity,
                                  const char* message,
                                  const size_t length) {
  if (length == 0) {
    return;
  }

  Context& ctx = Context::Get();
  ctx.Write(severity, message, length);

  // Ensure the new line is logged.
  if (message[length - 1] != '\n') {
    ctx.Write(severity, "\n", 1);
  }

  ctx.Flush(severity);
}

// Format buffer using given format and arguments and log it it. The logging
// involves writing the formatted message via context's Write() and Flush().
inline void FormatAndLogArgumentList(const Severity severity,
                                     const char* format,
                                     std::va_list vlist) {
  Context& ctx = Context::Get();

  // Allocate buffer for formatting.
  size_t buffer_size = 0;
  char* buffer = ctx.AllocatePrintBuffer(buffer_size);
  if (!buffer_size || !buffer_size) {
    return;
  }

  // std::vsnprintf() returns the number of characters written if successful or
  // negative value if an error occurred. If the resulting string gets truncated
  // due to buf_size limit, function returns the total number of characters (not
  // including the terminating null-byte) which would have been written, if the
  // limit was not imposed.
  const int length = std::vsnprintf(buffer, buffer_size, format, vlist);

  if (length > 0) {
    // The actual number of characters which was written to the buffer.
    const int written_length =
        (length < buffer_size) ? length : buffer_size - 1;

    LogMessageUnformatted(severity, buffer, written_length);
  }

  ctx.FreePrintBuffer(buffer, buffer_size);
}

}  // namespace internal

// Message which is being logged.
//
// This object takes care of providing an output stream for the API to log to,
// and it takes care of putting the message with a proper format to the logging
// output.
//
// A new line at the end of the logging message is ensured automatically.
// Every logged message flushes the logging output.
class Message {
 public:
  Message(const Severity severity) : stream_(severity) {}

  ~Message() {
    Flush();

    // Handle situation when the regular message has been constructed with fatal
    // severity: the fatal severity is expected to abort execution.
    if (stream_.GetSeverity() == Severity::kFatal) {
      Context::Get().Fail();
    }
  }

  // Get an output stream to be used as a destination of a logging message.
  auto GetStream() -> std::ostream& { return stream_; }

 protected:
  // Flush the stream after the message was fully put to the output stream.
  void Flush() { stream_.Flush(); }

  internal::BufferredStream stream_;
};

// Special implementation of the message logging which will abort the program
// execution after the message is logged.
class FatalMessage : public Message {
 public:
  FatalMessage() : Message(Severity::kFatal) {}

#if defined(_MSC_VER)
#  pragma warning(push)
// 4722 - Destructor never returns due to abort().
#  pragma warning(disable : 4722)
#endif

  [[noreturn]] ~FatalMessage() {
    Flush();
    Context::Get().Fail();
  }

#if defined(_MSC_VER)
#  pragma warning(pop)
#endif
};

// Log message of the given severity using printf style of logging function.
//
// The message denotes a printf-style format. The actual logger text will be
// truncated by the buffer size allocated by AllocatePrintBuffer().
//
// If the severity is Severity::kFatal then the message is logged, flushed, and
// execution is terminated via the Fail() function callback.
inline void LogFormatted(const Severity severity, const char* format, ...) {
  std::va_list args;
  va_start(args, format);
  internal::FormatAndLogArgumentList(severity, format, args);
  va_end(args);

  // Handle situation when the regular message has been constructed with fatal
  // severity: the fatal severity is expected to abort execution.
  if (severity == Severity::kFatal) {
    Context::Get().Fail();
  }
}

// Log message of the given severity without formatting: the text of the message
// is logged as-is.
//
// This is typically a less expensive way of logging messages for which no
// variable substitution is needed.
//
// If the severity is Severity::kFatal then the message is logged, flushed, and
// execution is terminated via the Fail() function callback.
inline void LogUnformatted(const Severity severity, const char* message) {
  internal::LogMessageUnformatted(severity, message, strlen(message));

  // Handle situation when the regular message has been constructed with fatal
  // severity: the fatal severity is expected to abort execution.
  if (severity == Severity::kFatal) {
    Context::Get().Fail();
  }
}

// Log fatal message using printf-style formatted print.
// The function will log the message, flush it, and execute the provided Fail()
// callback.
//
// Functionally it is the same as LogFormatted(Severity::kFatal, format, ...)
// but has a [[noreturn]] attribute.
[[noreturn]] inline void FatalFormatted(const char* format, ...) {
  std::va_list args;
  va_start(args, format);
  internal::FormatAndLogArgumentList(Severity::kFatal, format, args);
  va_end(args);

  Context::Get().Fail();
}

// Log fatal message of the given severity without formatting: the text of the
// message is logged as-is.
//
// The function will log the message, flush it, and execute the provided Fail()
// callback. This is typically a less expensive way of logging messages for
// which no variable substitution is needed.
//
// Functionally it is the same as LogFormatted(Severity::kFatal, format, ...)
// but has a [[noreturn]] attribute.
[[noreturn]] inline void FatalUnformatted(const char* message) {
  internal::LogMessageUnformatted(Severity::kFatal, message, strlen(message));

  Context::Get().Fail();
}

}  // namespace TL_LOG_VERSION_NAMESPACE
}  // namespace TL_LOG_NAMESPACE

#undef TL_LOG_VERSION_MAJOR
#undef TL_LOG_VERSION_MINOR
#undef TL_LOG_VERSION_REVISION

#undef TL_LOG_NAMESPACE

#undef TL_LOG_VERSION_NAMESPACE_CONCAT_HELPER
#undef TL_LOG_VERSION_NAMESPACE_CONCAT
#undef TL_LOG_VERSION_NAMESPACE

#undef TL_LOG_NO_DEFAULT_IMPLEMENTATION
#undef TL_LOG_UNREACHABLE_ABORT
