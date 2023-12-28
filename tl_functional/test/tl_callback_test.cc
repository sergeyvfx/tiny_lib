// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#include "tl_functional/tl_callback.h"

#include "tiny_lib/unittest/test.h"

namespace tiny_lib::callback {

TEST(Callback, DefaultIDConstructor) {
  using MyCallback = Callback<void(int)>;
  MyCallback my_callback;

  MyCallback::ID id;
  (void)id;
}

TEST(Callback, Basic) {
  using MyCallback = Callback<void(int)>;
  MyCallback my_callback;

  int counter = 0;

  my_callback(11);
  EXPECT_EQ(counter, 0);

  const MyCallback::ID id =
      my_callback.AddListener([&counter](int x) { counter += x; });

  my_callback(17);
  EXPECT_EQ(counter, 17);

  my_callback.RemoveListener(id);

  my_callback(19);
  EXPECT_EQ(counter, 17);
}

TEST(Callback, MultipleListeners) {
  using MyCallback = Callback<void(int)>;
  MyCallback my_callback;

  int counter = 0;

  my_callback(11);
  EXPECT_EQ(counter, 0);

  const MyCallback::ID id1 =
      my_callback.AddListener([&counter](int x) { counter += x; });
  const MyCallback::ID id2 =
      my_callback.AddListener([&counter](int x) { counter += x; });

  my_callback(17);
  EXPECT_EQ(counter, 34);

  my_callback.RemoveListener(id1);

  my_callback(19);
  EXPECT_EQ(counter, 53);

  my_callback.RemoveListener(id2);

  my_callback(21);
  EXPECT_EQ(counter, 53);
}

TEST(Callback, RemoveAllListeners) {
  using MyCallback = Callback<void(int)>;
  MyCallback my_callback;

  int counter = 0;

  my_callback(11);
  EXPECT_EQ(counter, 0);

  my_callback.AddListener([&counter](int x) { counter += x; });

  my_callback(17);
  EXPECT_EQ(counter, 17);

  my_callback.RemoveAllListeners();

  my_callback(19);
  EXPECT_EQ(counter, 17);
}

}  // namespace tiny_lib::callback
