// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#include "tl_result/tl_result.h"

#include "tiny_lib/unittest/test.h"

namespace tiny_lib::result {

enum class Error {
  kUnkown,
  kGenericError,
  kImpossibleError,
};

TEST(tl_result, Construct) {
  // Empty.
  {
    Result<int, Error> result(Error::kGenericError);
    EXPECT_FALSE(result.Ok());
    EXPECT_EQ(result.GetError(), Error::kGenericError);
    EXPECT_FALSE(result.HasValue());
  }

  // Initialize from rvalue.
  {
    Result<int, Error> result(20);
    EXPECT_TRUE(result.Ok());
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(result.GetValue(), 20);
  }

  // Initialize from rvalue with an error.
  {
    Result<int, Error> result(20, Error::kGenericError);
    EXPECT_FALSE(result.Ok());
    EXPECT_EQ(result.GetError(), Error::kGenericError);
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(result.GetValue(), 20);
  }
}

TEST(tl_result, ValueAccessViaOperator) {
  class Value {
   public:
    Value(int value) : int_value(value) {}

    int int_value;
  };

  {
    Result<Value, Error> result(10);
    EXPECT_EQ(result->int_value, 10);
    EXPECT_EQ((*result).int_value, 10);
  }

  {
    const Result<Value, Error> result(10);
    EXPECT_EQ(result->int_value, 10);
    EXPECT_EQ((*result).int_value, 10);
  }
}

TEST(tl_result, Boolean) {
  // Result with explicit value and implicit error code.
  {
    Result<int, bool> result(10);
    EXPECT_TRUE(result.Ok());
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(result.GetValue(), 10);
  }

  // Explicitly true result.
  {
    Result<int, bool> result(true);
    EXPECT_FALSE(result.Ok());
  }

  // Explicitly false result.
  {
    Result<int, bool> result(false);
    EXPECT_FALSE(result.Ok());
  }
}

}  // namespace tiny_lib::result
