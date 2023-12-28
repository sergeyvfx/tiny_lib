// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#include "tl_container/tl_static_vector.h"

#include <array>
#include <string>
#include <vector>

#include "tiny_lib/unittest/mock.h"
#include "tiny_lib/unittest/test.h"

// When it is defined to 1 the test will run tests against the std::vector.
// This is used to align the behavior of the StaticVector with the STL.
#define USE_STD_VECTOR_REFERENCE 0

namespace tiny_lib::static_vector {

// Wrap the test into its own namespace, making it easy to define local
// StaticVector class which is an alias to std::string.
namespace test {

using testing::ElementsAre;

#if USE_STD_VECTOR_REFERENCE
template <class T, std::size_t N>
using StaticVector = std::vector<T>;
#endif

////////////////////////////////////////////////////////////////////////////////
// Member functions.

TEST(StaticVector, Construct) {
  // Default constructor.
  {
    StaticVector<int, 10> vec;
    EXPECT_EQ(vec.size(), 0);
  }

  // Construct the container with count copies of elements with value value.

  {
    StaticVector<int, 10> vec(3, 17);
    EXPECT_EQ(vec.size(), 3);
    EXPECT_THAT(vec, ElementsAre(17, 17, 17));
  }

  if (!USE_STD_VECTOR_REFERENCE) {
    using IntStaticVector = StaticVector<int, 10>;
    EXPECT_THROW_OR_ABORT(IntStaticVector(11, 17), std::length_error);
  }

  // Constructs the container with count default-inserted instances of T.

  {
    StaticVector<int, 10> vec(3);
    EXPECT_EQ(vec.size(), 3);
    EXPECT_THAT(vec, ElementsAre(0, 0, 0));
  }

  if (!USE_STD_VECTOR_REFERENCE) {
    using IntStaticVector = StaticVector<int, 10>;
    EXPECT_THROW_OR_ABORT(IntStaticVector(11), std::length_error);
  }

  // Constructs the container with the contents of the range [first, last).

  {
    const std::array<int, 3> data = {1, 2, 3};
    StaticVector<int, 10> vec(data.begin(), data.end());
    EXPECT_EQ(vec.size(), 3);
    EXPECT_THAT(vec, ElementsAre(1, 2, 3));
  }

  if (!USE_STD_VECTOR_REFERENCE) {
    using IntStaticVector = StaticVector<int, 10>;
    const std::array<int, 11> data({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});
    EXPECT_THROW_OR_ABORT(IntStaticVector(data.begin(), data.end()),
                          std::length_error);
  }

  // Copy constructor.

  {
    const std::array<int, 3> data = {1, 2, 3};
    const StaticVector<int, 10> other(data.begin(), data.end());
    StaticVector<int, 10> vec(other);
    EXPECT_EQ(vec.size(), 3);
    EXPECT_THAT(vec, ElementsAre(1, 2, 3));
  }

  // Move constructor.

  if (!USE_STD_VECTOR_REFERENCE) {
    const std::array<int, 3> data = {1, 2, 3};
    StaticVector<int, 10> other(data.begin(), data.end());
    StaticVector<int, 10> vec(std::move(other));
    // EXPECT_TRUE(other.empty());
    EXPECT_EQ(vec.size(), 3);
    EXPECT_THAT(vec, ElementsAre(1, 2, 3));
  }

  // Constructs the container with the contents of the initializer list.

  {
    const std::initializer_list<int> ilist = {1, 2, 3};
    StaticVector<int, 10> vec(ilist);
    EXPECT_EQ(vec.size(), 3);
    EXPECT_THAT(vec, ElementsAre(1, 2, 3));
  }
}

TEST(StaticVector, assign_operator) {
  // Copy assignment.

  {
    const StaticVector<int, 10> a({1, 2, 3, 4});
    StaticVector<int, 10> vec({5, 6, 7});

    vec = a;
    EXPECT_EQ(vec.size(), 4);
    EXPECT_THAT(vec, ElementsAre(1, 2, 3, 4));
  }

  // Move assignment.

  {
    StaticVector<int, 10> a({1, 2, 3, 4});
    StaticVector<int, 10> vec({5, 6, 7});

    vec = std::move(a);
    // EXPECT_EQ(a.size(), 0);
    EXPECT_EQ(vec.size(), 4);
    EXPECT_THAT(vec, ElementsAre(1, 2, 3, 4));
  }
}

TEST(StaticVector, assign) {
  // Replace the contents with count copies of value value.

  {
    StaticVector<int, 10> vec({1, 2, 3});
    vec.assign(4, 7);
    EXPECT_EQ(vec.size(), 4);
    EXPECT_THAT(vec, ElementsAre(7, 7, 7, 7));
  }

  if (!USE_STD_VECTOR_REFERENCE) {
    StaticVector<int, 10> vec({1, 2, 3});
    EXPECT_THROW_OR_ABORT(vec.assign(11, 7), std::length_error);
    EXPECT_EQ(vec.size(), 3);
    EXPECT_THAT(vec, ElementsAre(1, 2, 3));
  }

  // Replaces the contents with copies of those in the range [first, last).

  {
    const std::array<int, 4> data({4, 5, 6, 7});
    StaticVector<int, 10> vec({1, 2, 3});
    vec.assign(data.begin(), data.end());
    EXPECT_EQ(vec.size(), 4);
    EXPECT_THAT(vec, ElementsAre(4, 5, 6, 7));
  }

  if (!USE_STD_VECTOR_REFERENCE) {
    const std::array<int, 11> data({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});
    StaticVector<int, 10> vec({21, 22, 23});
    EXPECT_THROW_OR_ABORT(vec.assign(data.begin(), data.end()),
                          std::length_error);
    EXPECT_EQ(vec.size(), 3);
    EXPECT_THAT(vec, ElementsAre(21, 22, 23));
  }

  // Replaces the contents with the elements from the initializer list ilist.

  {
    const std::initializer_list<int> data{4, 5, 6, 7};
    StaticVector<int, 10> vec({1, 2, 3});
    vec.assign(data.begin(), data.end());
    EXPECT_EQ(vec.size(), 4);
    EXPECT_THAT(vec, ElementsAre(4, 5, 6, 7));
  }

  if (!USE_STD_VECTOR_REFERENCE) {
    const std::initializer_list<int> data{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    StaticVector<int, 10> vec({21, 22, 23});
    EXPECT_THROW_OR_ABORT(vec.assign(data.begin(), data.end()),
                          std::length_error);
    EXPECT_EQ(vec.size(), 3);
    EXPECT_THAT(vec, ElementsAre(21, 22, 23));
  }
}

// Element access
// ==============

TEST(StaticVector, at) {
  {
    StaticVector<int, 10> vec{1, 2, 3, 4};

    EXPECT_EQ(vec.at(1), 2);
    EXPECT_THROW_OR_ABORT(vec.at(4), std::out_of_range);
  }

  {
    const StaticVector<int, 10> vec{1, 2, 3, 4};

    EXPECT_EQ(vec.at(1), 2);
    EXPECT_THROW_OR_ABORT(vec.at(4), std::out_of_range);
  }
}

TEST(StaticVector, subscript_operator) {
  {
    StaticVector<int, 10> vec{1, 2, 3, 4};
    EXPECT_EQ(vec[1], 2);
  }

  {
    const StaticVector<int, 10> vec{1, 2, 3, 4};
    EXPECT_EQ(vec[1], 2);
  }
}

TEST(StaticVector, front) {
  {
    StaticVector<int, 10> vec{1, 2, 3, 4};
    EXPECT_EQ(vec.front(), 1);
  }

  {
    const StaticVector<int, 10> vec{1, 2, 3, 4};
    EXPECT_EQ(vec.front(), 1);
  }
}

TEST(StaticVector, back) {
  {
    StaticVector<int, 10> vec{1, 2, 3, 4};
    EXPECT_EQ(vec.back(), 4);
  }

  {
    const StaticVector<int, 10> vec{1, 2, 3, 4};
    EXPECT_EQ(vec.back(), 4);
  }
}

TEST(StaticVector, data) {
  StaticVector<int, 10> vec{1, 2, 3, 4};

  int* data = vec.data();
  EXPECT_NE(data, nullptr);
  EXPECT_EQ(data[0], 1);
  EXPECT_EQ(data[1], 2);
  EXPECT_EQ(data[2], 3);
  EXPECT_EQ(data[3], 4);
}

// Iterator
// ========

TEST(StaticVector, iterators) {
  StaticVector<int, 10> vec{1, 2, 3, 4};

  EXPECT_EQ(*vec.begin(), 1);
  EXPECT_EQ(*std::prev(vec.end()), 4);

  EXPECT_EQ(*vec.rbegin(), 4);
  EXPECT_EQ(*std::prev(vec.rend()), 1);
}

// Capacity
// ========

TEST(StaticVector, empty) {
  using IntStaticVector = StaticVector<int, 10>;

  EXPECT_TRUE(IntStaticVector().empty());
  EXPECT_FALSE(IntStaticVector(3).empty());
}

TEST(StaticVector, size) {
  using IntStaticVector = StaticVector<int, 10>;

  EXPECT_EQ(IntStaticVector().size(), 0);
  EXPECT_EQ(IntStaticVector(3).size(), 3);
}

TEST(StaticVector, max_size) {
  using IntStaticVector = StaticVector<int, 10>;

  if (!USE_STD_VECTOR_REFERENCE) {
    EXPECT_EQ(IntStaticVector().max_size(), 10);
  }
}

TEST(StaticVector, reserve) {
  {
    StaticVector<int, 10> vec;

    vec.reserve(10);
    EXPECT_EQ(vec.capacity(), 10);
  }

  if (!USE_STD_VECTOR_REFERENCE) {
    StaticVector<int, 10> vec({1, 2, 3});
    EXPECT_THROW_OR_ABORT(vec.reserve(11), std::length_error);
    EXPECT_EQ(vec.size(), 3);
    EXPECT_THAT(vec, ElementsAre(1, 2, 3));
  }
}

TEST(StaticVector, capacity) {
  using IntStaticVector = StaticVector<int, 10>;

  if (!USE_STD_VECTOR_REFERENCE) {
    EXPECT_EQ(IntStaticVector().capacity(), 10);
  }
}

TEST(StaticVector, shrink_to_fit) {
  if (!USE_STD_VECTOR_REFERENCE) {
    StaticVector<int, 10> vec;

    EXPECT_EQ(vec.capacity(), 10);
    vec.shrink_to_fit();
    EXPECT_EQ(vec.capacity(), 10);
  }
}

// Modifiers
// =========

TEST(StaticVector, clear) {
  StaticVector<std::string, 10> vec({"Hello", "World"});
  EXPECT_THAT(vec, ElementsAre("Hello", "World"));
  vec.clear();
  EXPECT_TRUE(vec.empty());
}

TEST(StaticVector, insert) {
  using Vector = StaticVector<std::string, 10>;

  // Insert single element, copy semantic.

  {
    const std::string a{"a"};
    const std::string b{"b"};
    const std::string c{"c"};

    Vector vec{"foo", "bar", "baz"};

    EXPECT_EQ(vec.insert(vec.begin(), a), vec.begin());
    EXPECT_THAT(vec, ElementsAre("a", "foo", "bar", "baz"));

    EXPECT_EQ(vec.insert(vec.end(), b), vec.begin() + 4);
    EXPECT_THAT(vec, ElementsAre("a", "foo", "bar", "baz", "b"));

    EXPECT_EQ(vec.insert(vec.begin() + 2, c), vec.begin() + 2);
    EXPECT_THAT(vec, ElementsAre("a", "foo", "c", "bar", "baz", "b"));
  }

  if (!USE_STD_VECTOR_REFERENCE) {
    const std::string b{"b"};
    Vector vec(10, "a");
    EXPECT_THROW_OR_ABORT(vec.insert(vec.begin(), b), std::length_error);
    EXPECT_THAT(vec,
                ElementsAre("a", "a", "a", "a", "a", "a", "a", "a", "a", "a"));
  }

  // Insert single element, move semantic.

  {
    std::string a{"a"};
    std::string b{"b"};
    std::string c{"c"};

    Vector vec;

    vec.insert(vec.begin(), "baz");
    vec.insert(vec.begin(), "bar");
    vec.insert(vec.begin(), "foo");

    EXPECT_EQ(vec.insert(vec.begin(), std::move(a)), vec.begin());
    EXPECT_THAT(vec, ElementsAre("a", "foo", "bar", "baz"));

    EXPECT_EQ(vec.insert(vec.end(), std::move(b)), vec.begin() + 4);
    EXPECT_THAT(vec, ElementsAre("a", "foo", "bar", "baz", "b"));

    EXPECT_EQ(vec.insert(vec.begin() + 2, std::move(c)), vec.begin() + 2);
    EXPECT_THAT(vec, ElementsAre("a", "foo", "c", "bar", "baz", "b"));
  }

  if (!USE_STD_VECTOR_REFERENCE) {
    std::string b{"b"};
    Vector vec(10, "a");
    EXPECT_THROW_OR_ABORT(vec.insert(vec.begin(), std::move(b)),
                          std::length_error);
    EXPECT_THAT(vec,
                ElementsAre("a", "a", "a", "a", "a", "a", "a", "a", "a", "a"));
  }

  // Insert multiple copies of the given value

  {
    Vector vec{"foo", "bar", "baz"};

    EXPECT_EQ(vec.insert(vec.begin(), 2, "a"), vec.begin());
    EXPECT_THAT(vec, ElementsAre("a", "a", "foo", "bar", "baz"));

    EXPECT_EQ(vec.insert(vec.end(), 2, "b"), vec.begin() + 5);
    EXPECT_THAT(vec, ElementsAre("a", "a", "foo", "bar", "baz", "b", "b"));

    EXPECT_EQ(vec.insert(vec.begin() + 3, 2, "c"), vec.begin() + 3);
    EXPECT_THAT(vec,
                ElementsAre("a", "a", "foo", "c", "c", "bar", "baz", "b", "b"));
  }

  {
    Vector vec{"foo", "bar"};
    vec.insert(vec.begin() + 1, 6, "a");
    EXPECT_THAT(vec, ElementsAre("foo", "a", "a", "a", "a", "a", "a", "bar"));
  }

  if (!USE_STD_VECTOR_REFERENCE) {
    Vector vec(9, "a");
    EXPECT_THROW_OR_ABORT(vec.insert(vec.end(), 2, "b"), std::length_error);
    EXPECT_THAT(vec, ElementsAre("a", "a", "a", "a", "a", "a", "a", "a", "a"));
  }

  // Insert range.

  {
    const std::array<std::string, 6> data({"a", "b", "c", "d", "e", "f"});
    Vector vec{"foo", "bar", "baz"};
    EXPECT_EQ(vec.insert(vec.begin() + 1, data.begin(), data.end()),
              vec.begin() + 1);
    EXPECT_THAT(vec,
                ElementsAre("foo", "a", "b", "c", "d", "e", "f", "bar", "baz"));
  }

  if (!USE_STD_VECTOR_REFERENCE) {
    const std::array<std::string, 6> data({"a", "b", "c", "d", "e", "f"});
    Vector vec(9, "a");
    EXPECT_THROW_OR_ABORT(
        vec.insert(std::prev(vec.end()), data.begin(), data.end()),
        std::length_error);
    EXPECT_THAT(vec, ElementsAre("a", "a", "a", "a", "a", "a", "a", "a", "a"));
  }

  // Insert initializer list ran.

  {
    Vector vec{"foo", "bar", "baz"};
    EXPECT_EQ(vec.insert(vec.begin() + 1, {"a", "b", "c", "d", "e", "f"}),
              vec.begin() + 1);
    EXPECT_THAT(vec,
                ElementsAre("foo", "a", "b", "c", "d", "e", "f", "bar", "baz"));
  }

  if (!USE_STD_VECTOR_REFERENCE) {
    Vector vec(9, "a");
    EXPECT_THROW_OR_ABORT(
        vec.insert(std::prev(vec.end()), {"a", "b", "c", "d", "e", "f"}),
        std::length_error);
    EXPECT_THAT(vec, ElementsAre("a", "a", "a", "a", "a", "a", "a", "a", "a"));
  }
}

TEST(StaticVector, emplace) {
  using Vector = StaticVector<std::string, 10>;

  {
    Vector vec{"foo", "bar", "baz"};

    vec.emplace(vec.begin() + 1, 2, 'a');
    EXPECT_THAT(vec, ElementsAre("foo", "aa", "bar", "baz"));
  }

  if (!USE_STD_VECTOR_REFERENCE) {
    const std::string b{"b"};
    Vector vec(10, "a");
    EXPECT_THROW_OR_ABORT(vec.emplace(vec.begin() + 1, 2, 'a'),
                          std::length_error);
    EXPECT_THAT(vec,
                ElementsAre("a", "a", "a", "a", "a", "a", "a", "a", "a", "a"));
  }
}

TEST(StaticVector, erase) {
  using Vector = StaticVector<std::string, 10>;

  // Erase single element.

  {
    Vector vec{"a", "b", "c", "d"};

    EXPECT_EQ(vec.erase(vec.begin() + 1), vec.begin() + 1);
    EXPECT_EQ(vec.size(), 3);
    EXPECT_THAT(vec, ElementsAre("a", "c", "d"));
  }

  // Erase multiple elements.

  {
    Vector vec{"a", "b", "c", "d"};

    EXPECT_EQ(vec.erase(vec.begin() + 1, vec.end()), vec.begin() + 1);
    EXPECT_EQ(vec.size(), 1);
    EXPECT_THAT(vec, ElementsAre("a"));
  }
}

TEST(StaticVector, swap) {
  using Vector = StaticVector<std::string, 10>;

  Vector a{"a", "b", "c"};
  Vector b{"1", "2", "3", "4"};

  a.swap(b);
  EXPECT_THAT(a, ElementsAre("1", "2", "3", "4"));
  EXPECT_THAT(b, ElementsAre("a", "b", "c"));

  a.swap(b);
  EXPECT_THAT(a, ElementsAre("a", "b", "c"));
  EXPECT_THAT(b, ElementsAre("1", "2", "3", "4"));
}

TEST(StaticVector, push_back) {
  // Copy semantic.

  {
    const std::string hello{"Hello"};
    const std::string world{"World"};
    StaticVector<std::string, 10> vec;
    vec.push_back(hello);
    vec.push_back(world);
    EXPECT_EQ(vec.size(), 2);
    EXPECT_THAT(vec, ElementsAre("Hello", "World"));
  }

  if (!USE_STD_VECTOR_REFERENCE) {
    const std::string foo{"foo"};
    StaticVector<std::string, 2> vec{"Hello", "World"};
    EXPECT_THROW_OR_ABORT(vec.push_back(foo), std::length_error);
    EXPECT_EQ(vec.size(), 2);
    EXPECT_THAT(vec, ElementsAre("Hello", "World"));
  }

  // Move semantic.

  {
    std::string hello{"Hello"};
    std::string world{"World"};
    StaticVector<std::string, 10> vec;
    vec.push_back(std::move(hello));
    vec.push_back(std::move(world));
    EXPECT_EQ(vec.size(), 2);
    EXPECT_THAT(vec, ElementsAre("Hello", "World"));
  }

  if (!USE_STD_VECTOR_REFERENCE) {
    std::string foo{"foo"};
    StaticVector<std::string, 2> vec{"Hello", "World"};
    EXPECT_THROW_OR_ABORT(vec.push_back(std::move(foo)), std::length_error);
    EXPECT_EQ(vec.size(), 2);
    EXPECT_THAT(vec, ElementsAre("Hello", "World"));
  }
}

TEST(StaticVector, emplace_back) {
  StaticVector<std::string, 10> vec;
  vec.emplace_back(3, 'a');
  vec.emplace_back(3, 'b');
  EXPECT_EQ(vec.size(), 2);
  EXPECT_THAT(vec, ElementsAre("aaa", "bbb"));
}

TEST(StaticVector, pop_back) {
  StaticVector<std::string, 10> vec{"foo", "bar", "baz"};
  vec.pop_back();
  EXPECT_EQ(vec.size(), 2);
  EXPECT_THAT(vec, ElementsAre("foo", "bar"));
}

TEST(StaticVector, resize) {
  // Default constructor.

  // Grow.
  {
    StaticVector<std::string, 10> vec({"foo", "bar"});
    vec.resize(5);
    EXPECT_EQ(vec.size(), 5);
    EXPECT_THAT(vec, ElementsAre("foo", "bar", "", "", ""));
  }

  // Shrink.
  {
    StaticVector<std::string, 10> vec({"foo", "bar", "baz"});
    vec.resize(1);
    EXPECT_EQ(vec.size(), 1);
    EXPECT_THAT(vec, ElementsAre("foo"));
  }

  // Value initialization.

  // Grow.
  {
    StaticVector<std::string, 10> vec({"foo", "bar"});
    vec.resize(5, "baz");
    EXPECT_EQ(vec.size(), 5);
    EXPECT_THAT(vec, ElementsAre("foo", "bar", "baz", "baz", "baz"));
  }

  // Shrink.
  {
    StaticVector<std::string, 10> vec({"foo", "bar", "baz"});
    vec.resize(1, "baz");
    EXPECT_EQ(vec.size(), 1);
    EXPECT_THAT(vec, ElementsAre("foo"));
  }
}

////////////////////////////////////////////////////////////////////////////////
// Non-member functions.

TEST(StaticVector, compare) {
  // The test follows
  //   https://en.cppreference.com/w/cpp/container/vector/operator_cmp

  using Vector = StaticVector<int, 10>;

  Vector alice{1, 2, 3};
  Vector bob{7, 8, 9, 10};
  Vector eve{1, 2, 3};

  EXPECT_FALSE(alice == bob);
  EXPECT_TRUE(alice != bob);
  EXPECT_TRUE(alice < bob);
  EXPECT_TRUE(alice <= bob);
  EXPECT_FALSE(alice > bob);
  EXPECT_FALSE(alice >= bob);

  EXPECT_TRUE(alice == eve);
  EXPECT_FALSE(alice != eve);
  EXPECT_FALSE(alice < eve);
  EXPECT_TRUE(alice <= eve);
  EXPECT_FALSE(alice > eve);
  EXPECT_TRUE(alice >= eve);
}

TEST(StaticVector, swap_non_member) {
  using Vector = StaticVector<std::string, 10>;

  Vector a{"a", "b", "c"};
  Vector b{"1", "2", "3", "4"};

  swap(a, b);
  EXPECT_THAT(a, ElementsAre("1", "2", "3", "4"));
  EXPECT_THAT(b, ElementsAre("a", "b", "c"));

  swap(a, b);
  EXPECT_THAT(a, ElementsAre("a", "b", "c"));
  EXPECT_THAT(b, ElementsAre("1", "2", "3", "4"));
}

TEST(StaticVector, erase_non_member) {
  using Vector = StaticVector<int, 10>;

  Vector vec{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  EXPECT_EQ(erase(vec, 3), 1);
  EXPECT_THAT(vec, ElementsAre(0, 1, 2, 4, 5, 6, 7, 8, 9));
}

TEST(StaticVector, erase_if) {
  using Vector = StaticVector<int, 10>;

  Vector vec{0, 1, 2, 4, 5, 6, 7, 8, 9};

  EXPECT_EQ(erase_if(vec, [](char x) { return x % 2 == 0; }), 5);
  EXPECT_THAT(vec, ElementsAre(1, 5, 7, 9));
}

////////////////////////////////////////////////////////////////////////////////
// Maintenance.

// Avoid accident of a reference test committed.
TEST(StaticVector, IsReal) { EXPECT_EQ(USE_STD_VECTOR_REFERENCE, 0); }

}  // namespace test

}  // namespace tiny_lib::static_vector
