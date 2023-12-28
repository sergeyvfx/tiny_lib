// Copyright (c) 2021 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#pragma once

#include <filesystem>

// NOTE: Needs to be before include of GTest/GMock.
#include "tiny_lib/unittest/internal/test-internal.h"

#include <gtest/gtest.h>

namespace tiny_lib::testing {

using namespace ::testing;  // NOLINT

// Construct a fully qualified path for the test file.
auto TestFileAbsolutePath(const std::filesystem::path& filename)
    -> std::filesystem::path;

}  // namespace tiny_lib::testing

// If the exceptions are enabled EXPECT_THROW_OR_ABORT expects that the given
// exception is thrown during evaluation of the statement. If the exceptions are
// disabled then expects that the execution of the program is aborted.
#if defined(__cpp_exceptions) && __cpp_exceptions >= 199711L
#  define EXPECT_THROW_OR_ABORT(statement, expected_exception)                 \
    EXPECT_THROW(statement, expected_exception)
#else
#  define EXPECT_THROW_OR_ABORT(statement, expected_exception)                 \
    EXPECT_DEATH(statement, "")
#endif
