// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#include "tl_log/tl_log.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "tiny_lib/unittest/test.h"

namespace tiny_lib::log {

class BaseFunctions : public Functions {
 public:
  [[noreturn]] void Fail() override { abort(); }

  auto AllocatePrintBuffer(size_t& buffer_size) -> char* override {
    constexpr size_t kBufferSize = 1024;
    buffer_size = kBufferSize;
    return new char[kBufferSize];
  }

  void FreePrintBuffer(char* buffer, const size_t /*buffer_size*/) override {
    delete[] buffer;
  }
};

class StringFunctions : public BaseFunctions {
 public:
  StringFunctions(std::string& str) : str_(str) {}

  void Write(const Severity /*severity*/,
             const char* message,
             const size_t length) override {
    str_ += std::string_view(message, length);
  }

  void Flush(const Severity /*severity*/) override {}

 private:
  std::string& str_;
};

class StreamFunctions : public BaseFunctions {
 public:
  void Write(Severity severity,
             const char* message,
             const size_t length) override {
    FILE* stream = GetStreamForSeverity(severity);

    constexpr size_t kMaxNumCharsPerPrint = size_t(10) * 1024 * 1024;

    const char* ptr = message;
    size_t num_chars_printed = 0;
    while (num_chars_printed < length) {
      size_t num_chars_to_print = length - num_chars_printed;
      if (num_chars_to_print > kMaxNumCharsPerPrint) {
        num_chars_to_print = kMaxNumCharsPerPrint;
      }

      fprintf(stream, "%.*s", int(num_chars_to_print), ptr);

      ptr += num_chars_to_print;
      num_chars_printed += num_chars_to_print;
    }
  }

  void Flush(Severity severity) override {
    FILE* stream = GetStreamForSeverity(severity);
    fflush(stream);
  }

 private:
  static auto GetStreamForSeverity(const Severity severity) -> FILE* {
    // NOLINTBEGIN(bugprone-branch-clone)
    switch (severity) {
      case Severity::kInfo: return stdout;
      case Severity::kWarning: return stdout;
      case Severity::kError: return stderr;
      case Severity::kFatal: return stderr;
    }
    // NOLINTEND(bugprone-branch-clone)
    return nullptr;
  }
};

class LogTest : public ::testing::Test {
 protected:
  void TearDown() override { Context::Get().Reset(); }
};

TEST_F(LogTest, NonConfigured) {
  Message(Severity::kInfo).GetStream() << "Hello, World!";

  EXPECT_DEATH(
      { FatalMessage().GetStream() << "Fatal error!"; }, "Fatal error!\n");
}

////////////////////////////////////////////////////////////////////////////////
// Streamed logging API.

class LogStreamTest : public LogTest {};

TEST_F(LogStreamTest, Simple) {
  std::string log;
  StringFunctions functions(log);
  Context::Get().SetFunctions(functions);

  Message(Severity::kInfo).GetStream() << "Hello, World!";

  EXPECT_EQ(log, "Hello, World!\n");
}

TEST_F(LogStreamTest, NewLine) {
  std::string log;
  StringFunctions functions(log);
  Context::Get().SetFunctions(functions);

  Message(Severity::kInfo).GetStream() << "Hello, World!\n";

  EXPECT_EQ(log, "Hello, World!\n");
}

TEST_F(LogStreamTest, Nested) {
  std::string log;
  StringFunctions functions(log);
  Context::Get().SetFunctions(functions);

  auto logging_function = []() -> const char* {
    Message(Severity::kInfo).GetStream() << "foo";
    return "bar";
  };

  Message(Severity::kInfo).GetStream()
      << "Hello " << logging_function() << " World";

  // Follows the Google logging module behavior.
  EXPECT_EQ(log, "foo\nHello bar World\n");
}

TEST_F(LogStreamTest, Fatal) {
  StreamFunctions functions;
  Context::Get().SetFunctions(functions);

  EXPECT_DEATH(
      { Message(Severity::kFatal).GetStream() << "Fatal error!"; },
      "Fatal error!\n");

  EXPECT_DEATH(
      { FatalMessage().GetStream() << "Fatal error!"; }, "Fatal error!\n");
}

////////////////////////////////////////////////////////////////////////////////
// Logging to a null sync.

class NullFunctions : public Functions {
 public:
  void Write(const Severity /*severity*/,
             const char* /*message*/,
             const size_t /*length*/) override {}
  void Flush(Severity /*severity*/) override {}

  [[noreturn]] void Fail() override { abort(); }

  auto AllocatePrintBuffer(size_t& buffer_size) -> char* override {
    buffer_size = 0;
    return nullptr;
  }

  void FreePrintBuffer(char* /*buffer*/,
                       const size_t /*buffer_size*/) override {}
};

class LogNullTest : public LogTest {
 protected:
  void SetUp() override { Context::Get().SetFunctions(functions_); }

 private:
  NullFunctions functions_;
};

TEST_F(LogNullTest, Stream) {
  Message(Severity::kInfo).GetStream() << "Logging::Basic test";
  Message(Severity::kInfo).GetStream() << "Hello, World";
}

TEST_F(LogNullTest, StreamFatal) {
  EXPECT_DEATH(
      { Message(Severity::kFatal).GetStream() << "Fatal error!"; }, "");

  EXPECT_DEATH({ FatalMessage().GetStream() << "Fatal error!"; }, "");
}

TEST_F(LogNullTest, Formatted) {
  LogFormatted(Severity::kInfo, "Hello, %s!\n", "World");
}

TEST_F(LogNullTest, FormattedFatal) {
  EXPECT_DEATH(
      { LogFormatted(Severity::kFatal, "Hello, %s!\n", "World"); }, "");
}

TEST_F(LogNullTest, Unformatted) {
  LogUnformatted(Severity::kInfo, "Hello, World!\n");
}

TEST_F(LogNullTest, UnformattedFatal) {
  EXPECT_DEATH({ LogUnformatted(Severity::kFatal, "Hello, World!\n"); }, "");
}

////////////////////////////////////////////////////////////////////////////////
// Formatted logging API.

class LogFormattedTest : public LogTest {};

TEST_F(LogFormattedTest, Basic) {
  std::string log;
  StringFunctions functions(log);
  Context::Get().SetFunctions(functions);

  LogFormatted(Severity::kInfo, "Hello, %s!", "World");

  EXPECT_EQ(log, "Hello, World!\n");
}

TEST_F(LogFormattedTest, NewLine) {
  std::string log;
  StringFunctions functions(log);
  Context::Get().SetFunctions(functions);

  LogFormatted(Severity::kInfo, "Hello, %s!\n", "World");

  EXPECT_EQ(log, "Hello, World!\n");
}

TEST_F(LogFormattedTest, Nested) {
  std::string log;
  StringFunctions functions(log);
  Context::Get().SetFunctions(functions);

  auto logging_function = []() -> const char* {
    LogFormatted(Severity::kInfo, "foo");
    return "bar";
  };

  LogFormatted(Severity::kInfo, "Hello %s World", logging_function());

  // Follows the Google logging module behavior.
  EXPECT_EQ(log, "foo\nHello bar World\n");
}

TEST_F(LogFormattedTest, Fatal) {
  StreamFunctions functions;
  Context::Get().SetFunctions(functions);

  EXPECT_DEATH(
      { LogFormatted(Severity::kFatal, "Fatal %s!", "error"); },
      "Fatal error!\n");

  EXPECT_DEATH({ FatalFormatted("Fatal %s!", "error"); }, "Fatal error!\n");
}

////////////////////////////////////////////////////////////////////////////////
// Unformatted logging API.

class LogUnformattedTest : public LogTest {};

TEST_F(LogUnformattedTest, Basic) {
  std::string log;
  StringFunctions functions(log);
  Context::Get().SetFunctions(functions);

  LogUnformatted(Severity::kInfo, "Hello, World!");

  EXPECT_EQ(log, "Hello, World!\n");
}

TEST_F(LogUnformattedTest, NewLine) {
  std::string log;
  StringFunctions functions(log);
  Context::Get().SetFunctions(functions);

  LogUnformatted(Severity::kInfo, "Hello, World!\n");

  EXPECT_EQ(log, "Hello, World!\n");
}

TEST_F(LogUnformattedTest, Fatal) {
  StreamFunctions functions;
  Context::Get().SetFunctions(functions);

  EXPECT_DEATH(
      { LogUnformatted(Severity::kFatal, "Fatal error!"); }, "Fatal error!\n");

  EXPECT_DEATH({ FatalUnformatted("Fatal error!"); }, "Fatal error!\n");
}

}  // namespace tiny_lib::log
