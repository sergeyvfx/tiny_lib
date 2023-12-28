// Copyright (c) 2022 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#include "tl_string/tl_cstring_view.h"

#include <sstream>

#include "tiny_lib/unittest/test.h"

// When it is defined to 1 the test will run tests against the std::string_view.
// This is used to align the behavior of the CStringView with the STL.
#define USE_STD_STRING_VIEW_REFERENCE 0

namespace tiny_lib::cstring_view {

// Wrap the test into its own namespace, making it easy to define local
// StaticString class which is an alias to std::string.
namespace test {

#if USE_STD_STRING_VIEW_REFERENCE
using CStringView = std::string_view;
#endif

////////////////////////////////////////////////////////////////////////////////
// Member functions.

// Constructor ans assignment
// ==========================

TEST(tl_cstring_view, Constructor) {
  // Empty.
  {
    CStringView sv;

    EXPECT_EQ(sv.data(), nullptr);
    EXPECT_EQ(sv.size(), 0);
  }

  // Character sequence.
  {
    const char* str = "Hello, World!";
    CStringView sv(str);

    EXPECT_EQ(sv.data(), str);
    EXPECT_EQ(sv.size(), 13);
  }

  {
    const std::string str("Hello, World!");

    CStringView sv(str);

    EXPECT_EQ(sv.data(), str.c_str());
    EXPECT_EQ(sv.size(), 13);
  }
}

// Iterators
// =========

TEST(tl_cstring_view, Iterator) {
  // ### Non-const iterator

  {
    CStringView str("abcdef");
    std::string s;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (CStringView::iterator it = str.begin(); it != str.end(); ++it) {
      s += *it;
    }
    EXPECT_EQ(s, "abcdef");
  }

  // ### Const iterator

  {
    const CStringView str("abcdef");
    std::string s;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (CStringView::const_iterator it = str.begin(); it != str.end(); ++it) {
      s += *it;
    }
    EXPECT_EQ(s, "abcdef");
  }

  {
    const CStringView str("abcdef");
    std::string s;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (CStringView::const_iterator it = str.cbegin(); it != str.cend();
         ++it) {
      s += *it;
    }
    EXPECT_EQ(s, "abcdef");
  }
}

TEST(tl_cstring_view, ReverseIterator) {
  // ### Non-const iterator

  {
    CStringView str("abcdef");
    std::string s;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (CStringView::reverse_iterator it = str.rbegin(); it != str.rend();
         ++it) {
      s += *it;
    }
    EXPECT_EQ(s, "fedcba");
  }

  // ### Const iterator

  {
    const CStringView str("abcdef");
    std::string s;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (CStringView::const_reverse_iterator it = str.rbegin();
         it != str.rend();
         ++it) {
      s += *it;
    }
    EXPECT_EQ(s, "fedcba");
  }

  {
    const CStringView str("abcdef");
    std::string s;
    // NOLINTNEXTLINE(modernize-loop-convert)
    for (CStringView::const_reverse_iterator it = str.crbegin();
         it != str.crend();
         ++it) {
      s += *it;
    }
    EXPECT_EQ(s, "fedcba");
  }
}

// Element access
// ==============

TEST(tl_cstring_view, at) {
  // Non-const string.
  {
    CStringView str("abcdef");
    EXPECT_EQ(str.at(1), 'b');
  }

  // Const string.
  {
    const CStringView str("abcdef");
    EXPECT_EQ(str.at(1), 'b');
  }

  // Out of bounds.
  {
    const CStringView str("abcdef");
    EXPECT_THROW_OR_ABORT(str.at(6), std::out_of_range);
  }
}

TEST(tl_cstring_view, operator_at) {
  // Non-const string.
  {
    CStringView str("abcdef");
    EXPECT_EQ(str[1], 'b');
    EXPECT_EQ(str[6], '\0');
  }

  // Const string.
  {
    const CStringView str("abcdef");
    EXPECT_EQ(str[1], 'b');
    EXPECT_EQ(str[6], '\0');
  }
}

TEST(tl_cstring_view, front) {
  // Non-const string.
  {
    CStringView str("abcdef");
    EXPECT_EQ(str.front(), 'a');
  }

  // Const string.
  {
    const CStringView str("abcdef");
    EXPECT_EQ(str.front(), 'a');
  }
}

TEST(tl_cstring_view, back) {
  // Non-const string.
  {
    CStringView str("abcdef");
    EXPECT_EQ(str.back(), 'f');
  }

  // Const string.
  {
    const CStringView str("abcdef");
    EXPECT_EQ(str.back(), 'f');
  }
}

TEST(tl_cstring_view, string_view) {
  CStringView str("abcdef");

  std::string_view sv{str};
  EXPECT_EQ(sv, "abcdef");
}

// Capacity
// ========

TEST(tl_cstring_view, empty) {
  EXPECT_TRUE(CStringView().empty());
  EXPECT_FALSE(CStringView("Hello, World!").empty());
}

TEST(tl_cstring_view, size) {
  EXPECT_EQ(CStringView().size(), 0);
  EXPECT_EQ(CStringView("x").size(), 1);
  EXPECT_EQ(CStringView("abc").size(), 3);
}

TEST(tl_cstring_view, length) {
  EXPECT_EQ(CStringView().length(), 0);
  EXPECT_EQ(CStringView("x").length(), 1);
  EXPECT_EQ(CStringView("abc").length(), 3);
}

// Modifiers
// =========

TEST(tl_cstring_view, swap) {
  CStringView a("foo");
  CStringView b("bar");

  a.swap(b);

  EXPECT_EQ(a, "bar");
  EXPECT_EQ(b, "foo");
}

// Operations
// ==========

TEST(tl_cstring_view, compare) {
  // Compare to a CStringView.
  EXPECT_EQ(CStringView("123").compare(CStringView("123")), 0);
  EXPECT_LT(CStringView("12").compare(CStringView("123")), 0);
  EXPECT_GT(CStringView("123").compare(CStringView("12")), 0);
  EXPECT_LT(CStringView("122").compare(CStringView("123")), 0);
  EXPECT_GT(CStringView("123").compare(CStringView("122")), 0);

  // Compare substring to a string view.
  EXPECT_EQ(CStringView("01234").compare(1, 3, CStringView("123")), 0);
  EXPECT_LT(CStringView("01234").compare(0, 3, CStringView("123")), 0);
  EXPECT_GT(CStringView("01234").compare(2, 3, CStringView("123")), 0);
  EXPECT_LT(CStringView("01234").compare(1, 2, CStringView("123")), 0);
  EXPECT_GT(CStringView("01234").compare(1, 4, CStringView("123")), 0);

  // Compare substring to a substring.
  EXPECT_EQ(CStringView("01234").compare(1, 3, CStringView("01234"), 1, 3), 0);
  EXPECT_EQ(CStringView("01234").compare(0, 3, CStringView("34012"), 2, 3), 0);
  EXPECT_LT(CStringView("01234").compare(1, 3, CStringView("01234"), 2, 3), 0);
  EXPECT_GT(CStringView("01234").compare(1, 3, CStringView("01234"), 1, 2), 0);

  // Compare to a null-terminated character string.
  EXPECT_EQ(CStringView("123").compare("123"), 0);
  EXPECT_LT(CStringView("12").compare("123"), 0);
  EXPECT_GT(CStringView("123").compare("12"), 0);
  EXPECT_LT(CStringView("122").compare("123"), 0);
  EXPECT_GT(CStringView("123").compare("122"), 0);

  // Compare substring to a null-terminated character string.
  EXPECT_EQ(CStringView("01234").compare(1, 3, "123"), 0);
  EXPECT_LT(CStringView("01234").compare(0, 3, "123"), 0);
  EXPECT_GT(CStringView("01234").compare(2, 3, "123"), 0);
  EXPECT_LT(CStringView("01234").compare(1, 2, "123"), 0);
  EXPECT_GT(CStringView("01234").compare(1, 4, "123"), 0);

  // Compare substring to a substring of a null-terminated character string.
  EXPECT_EQ(CStringView("01234").compare(1, 3, "1234", 3), 0);
  EXPECT_GT(CStringView("01234").compare(1, 3, "01234", 3), 0);
  EXPECT_LT(CStringView("01234").compare(1, 3, "23456", 3), 0);
}

TEST(tl_cstring_view, starts_with) {
  // Check for starts with a string.
  EXPECT_FALSE(CStringView("").starts_with(std::string("abc")));
  EXPECT_FALSE(CStringView("ab").starts_with(std::string("abc")));
  EXPECT_TRUE(CStringView("abc").starts_with(std::string("abc")));
  EXPECT_TRUE(CStringView("abcd").starts_with(std::string("abc")));
  EXPECT_FALSE(CStringView("xabcd").starts_with(std::string("abc")));
  EXPECT_TRUE(CStringView("").starts_with(std::string("")));
  EXPECT_TRUE(CStringView("abc").starts_with(std::string("")));

  // Check for starts with a character.
  EXPECT_FALSE(CStringView("").starts_with('x'));
  EXPECT_FALSE(CStringView("").starts_with('\0'));
  EXPECT_FALSE(CStringView("a").starts_with('\0'));
  EXPECT_FALSE(CStringView("abc").starts_with('x'));
  EXPECT_TRUE(CStringView("xabc").starts_with('x'));

  // Check for starts with a null-terminated character string.
  EXPECT_FALSE(CStringView("").starts_with("abc"));
  EXPECT_FALSE(CStringView("ab").starts_with("abc"));
  EXPECT_TRUE(CStringView("abc").starts_with("abc"));
  EXPECT_TRUE(CStringView("abcd").starts_with("abc"));
  EXPECT_TRUE(CStringView("").starts_with(""));
  EXPECT_TRUE(CStringView("abc").starts_with(""));
}

TEST(tl_cstring_view, ends_with) {
  // Check for ends with a string.
  EXPECT_FALSE(CStringView("").ends_with(std::string("abc")));
  EXPECT_FALSE(CStringView("ab").ends_with(std::string("abc")));
  EXPECT_TRUE(CStringView("abc").ends_with(std::string("abc")));
  EXPECT_FALSE(CStringView("abcd").ends_with(std::string("abc")));
  EXPECT_TRUE(CStringView("").ends_with(std::string("")));
  EXPECT_TRUE(CStringView("abc").ends_with(std::string("")));

  // Check for ends with a character.
  EXPECT_FALSE(CStringView("").ends_with('x'));
  EXPECT_FALSE(CStringView("").ends_with('\0'));
  EXPECT_FALSE(CStringView("a").ends_with('\0'));
  EXPECT_FALSE(CStringView("abc").ends_with('x'));
  EXPECT_TRUE(CStringView("abcx").ends_with('x'));

  // Check for ends with a null-terminated character string.
  EXPECT_FALSE(CStringView("").ends_with("abc"));
  EXPECT_FALSE(CStringView("ab").ends_with("abc"));
  EXPECT_TRUE(CStringView("abc").ends_with("abc"));
  EXPECT_FALSE(CStringView("abcd").ends_with("abc"));
  EXPECT_TRUE(CStringView("").ends_with(""));
  EXPECT_TRUE(CStringView("abc").ends_with(""));
}

#if !USE_STD_STRING_VIEW_REFERENCE  // Only available in C++23
TEST(tl_cstring_view, contains) {
  EXPECT_TRUE(CStringView("abcdef").contains(std::string("bcd")));
  EXPECT_FALSE(CStringView("abcdef").contains(std::string("xyz")));

  EXPECT_TRUE(CStringView("abcdef").contains('c'));
  EXPECT_FALSE(CStringView("abcdef").contains('x'));

  EXPECT_TRUE(CStringView("abcdef").contains("bcd"));
  EXPECT_FALSE(CStringView("abcdef").contains("xyz"));
}
#endif

TEST(tl_cstring_view, find) {
  EXPECT_EQ(CStringView("This is a string").find(CStringView("is")), 2);
  EXPECT_EQ(CStringView("This is a string").find(CStringView("is"), 4), 5);
  EXPECT_EQ(CStringView("This is a string").find(CStringView("foo")),
            CStringView::npos);

  EXPECT_EQ(CStringView("This is a string").find("isx", 0, 2), 2);
  EXPECT_EQ(CStringView("This is a string").find("isx", 4, 2), 5);
  EXPECT_EQ(CStringView("This is a string").find("foo", 4, 2),
            CStringView::npos);

  EXPECT_EQ(CStringView("This is a string").find("is", 0, 2), 2);
  EXPECT_EQ(CStringView("This is a string").find("is", 4, 2), 5);
  EXPECT_EQ(CStringView("This is a string").find("foo", 4, 2),
            CStringView::npos);

  EXPECT_EQ(CStringView("This is a string").find('i'), 2);
  EXPECT_EQ(CStringView("This is a string").find('i', 3), 5);
  EXPECT_EQ(CStringView("This is a string").find('x'), CStringView::npos);

  EXPECT_EQ(CStringView("This is a string").find(std::string("is")), 2);
  EXPECT_EQ(CStringView("This is a string").find(std::string("is"), 4), 5);
  EXPECT_EQ(CStringView("This is a string").find(std::string("foo")),
            CStringView::npos);
}

TEST(tl_cstring_view, rfind) {
  EXPECT_EQ(CStringView("This is a string").rfind(CStringView("is")), 5);
  EXPECT_EQ(CStringView("This is a string").rfind(CStringView("is"), 4), 2);
  EXPECT_EQ(CStringView("This is a string").rfind(CStringView("foo")),
            CStringView::npos);

  EXPECT_EQ(CStringView("This is a string").rfind("isx", 10, 2), 5);
  EXPECT_EQ(CStringView("This is a string").rfind("isx", 4, 2), 2);
  EXPECT_EQ(CStringView("This is a string").rfind("foo", 4, 2),
            CStringView::npos);

  EXPECT_EQ(CStringView("This is a string").rfind("is", 10, 2), 5);
  EXPECT_EQ(CStringView("This is a string").rfind("is", 4, 2), 2);
  EXPECT_EQ(CStringView("This is a string").rfind("foo", 6, 2),
            CStringView::npos);

  EXPECT_EQ(CStringView("This is a string").rfind('i'), 13);
  EXPECT_EQ(CStringView("This is a string").rfind('i', 8), 5);
  EXPECT_EQ(CStringView("This is a string").rfind('x'), CStringView::npos);

  EXPECT_EQ(CStringView("This is a string").rfind(std::string("is")), 5);
  EXPECT_EQ(CStringView("This is a string").rfind(std::string("is"), 4), 2);
  EXPECT_EQ(CStringView("This is a string").rfind(std::string("foo")),
            CStringView::npos);
}

TEST(tl_cstring_view, find_first_of) {
  // Test is based on C++ reference page.

  const char* buf = "xyzabc";

  EXPECT_EQ(CStringView("alignas").find_first_of(CStringView("klmn")), 1);
  EXPECT_EQ(CStringView("alignas").find_first_of(CStringView("klmn"), 2), 4);
  EXPECT_EQ(CStringView("alignas").find_first_of(CStringView("xyzw")),
            CStringView::npos);

  EXPECT_EQ(CStringView("consteval").find_first_of(buf, 0, 6), 0);
  EXPECT_EQ(CStringView("consteval").find_first_of(buf, 1, 6), 7);
  EXPECT_EQ(CStringView("consteval").find_first_of(buf, 0, 3),
            CStringView::npos);

  EXPECT_EQ(CStringView("decltype").find_first_of(buf), 2);
  EXPECT_EQ(CStringView("declvar").find_first_of(buf, 3), 5);
  EXPECT_EQ(CStringView("hello").find_first_of(buf), CStringView::npos);

  EXPECT_EQ(CStringView("co_await").find_first_of('a'), 3);
  EXPECT_EQ(CStringView("co_await").find_first_of('a', 4), 5);
  EXPECT_EQ(CStringView("co_await").find_first_of('x'), CStringView::npos);

  EXPECT_EQ(CStringView("constinit").find_first_of(std::string("int")), 2);
  EXPECT_EQ(CStringView("constinit").find_first_of(std::string("int"), 3), 4);
  EXPECT_EQ(CStringView("constinit").find_first_of(std::string("xyz")),
            CStringView::npos);
}

TEST(tl_cstring_view, find_first_not_of) {
  const char* buf = "xyzabc";

  EXPECT_EQ(CStringView("xyzUxVW").find_first_not_of(CStringView(buf)), 3);
  EXPECT_EQ(CStringView("xyzUxVW").find_first_not_of(CStringView(buf), 4), 5);
  EXPECT_EQ(CStringView("xyzxyz").find_first_not_of(CStringView(buf), 4),
            CStringView::npos);

  EXPECT_EQ(CStringView("xyzcxUW").find_first_not_of(buf, 0, 5), 3);
  EXPECT_EQ(CStringView("xyzcxUW").find_first_not_of(buf, 4, 5), 5);
  EXPECT_EQ(CStringView("xyzxyz").find_first_not_of(buf, 4, 5),
            CStringView::npos);

  EXPECT_EQ(CStringView("xyzUxVW").find_first_not_of(buf), 3);
  EXPECT_EQ(CStringView("xyzUxVW").find_first_not_of(buf, 4), 5);
  EXPECT_EQ(CStringView("xyzxyz").find_first_not_of(buf, 4), CStringView::npos);

  EXPECT_EQ(CStringView("xyxzabc").find_first_not_of('x'), 1);
  EXPECT_EQ(CStringView("xyxzabc").find_first_not_of('x', 2), 3);
  EXPECT_EQ(CStringView("www").find_first_not_of('w'), CStringView::npos);

  EXPECT_EQ(CStringView("xyzUxVW").find_first_not_of(std::string(buf)), 3);
  EXPECT_EQ(CStringView("xyzUxVW").find_first_not_of(std::string(buf), 4), 5);
  EXPECT_EQ(CStringView("xyzxyz").find_first_not_of(std::string(buf), 4),
            CStringView::npos);
}

TEST(tl_cstring_view, find_last_of) {
  // Test is based on C++ reference page.

  const char* buf = "xyzabc";

  EXPECT_EQ(CStringView("alignas").find_last_of(CStringView("klmn")), 4);
  EXPECT_EQ(CStringView("alignas").find_last_of(CStringView("klmn"), 3), 1);
  EXPECT_EQ(CStringView("alignas").find_last_of(CStringView("xyzw")),
            CStringView::npos);

  EXPECT_EQ(CStringView("consteval").find_last_of(buf, 8, 6), 7);
  EXPECT_EQ(CStringView("consteval").find_last_of(buf, 5, 6), 0);
  EXPECT_EQ(CStringView("consteval").find_last_of(buf, 0, 3),
            CStringView::npos);

  EXPECT_EQ(CStringView("decltype").find_last_of(buf), 5);
  EXPECT_EQ(CStringView("decltype").find_last_of(buf, 4), 2);
  EXPECT_EQ(CStringView("hello").find_last_of(buf), CStringView::npos);

  EXPECT_EQ(CStringView("co_await").find_last_of('a'), 5);
  EXPECT_EQ(CStringView("co_await").find_last_of('a', 4), 3);
  EXPECT_EQ(CStringView("co_await").find_last_of('x'), CStringView::npos);

  EXPECT_EQ(CStringView("constinit").find_last_of(std::string("int")), 8);
  EXPECT_EQ(CStringView("constinit").find_last_of(std::string("int"), 6), 6);
  EXPECT_EQ(CStringView("constinit").find_last_of(std::string("xyz")),
            CStringView::npos);
}

TEST(tl_cstring_view, find_last_not_of) {
  const char* buf = "xyzabc";

  EXPECT_EQ(CStringView("xyzUxVWx").find_last_not_of(CStringView(buf)), 6);
  EXPECT_EQ(CStringView("xyzUxVWx").find_last_not_of(CStringView(buf), 4), 3);
  EXPECT_EQ(CStringView("xyzxyz").find_last_not_of(CStringView(buf), 4),
            CStringView::npos);

  EXPECT_EQ(CStringView("xyzcxUcx").find_last_not_of(buf, 7, 5), 6);
  EXPECT_EQ(CStringView("xyzcxUcx").find_last_not_of(buf, 4, 4), 3);
  EXPECT_EQ(CStringView("xyzxyz").find_last_not_of(buf, 4, 5),
            CStringView::npos);

  EXPECT_EQ(CStringView("xyzUxVWc").find_last_not_of(buf), 6);
  EXPECT_EQ(CStringView("xyzUxxVWc").find_last_not_of(buf, 5), 3);
  EXPECT_EQ(CStringView("xyzxyz").find_last_not_of(buf, 4), CStringView::npos);

  EXPECT_EQ(CStringView("xyzabxcx").find_last_not_of('x'), 6);
  EXPECT_EQ(CStringView("xyzabxcx").find_last_not_of('x', 5), 4);
  EXPECT_EQ(CStringView("www").find_last_not_of('w'), CStringView::npos);

  EXPECT_EQ(CStringView("xyzUxVWx").find_last_not_of(std::string(buf)), 6);
  EXPECT_EQ(CStringView("xyzUxVWx").find_last_not_of(std::string(buf), 4), 3);
  EXPECT_EQ(CStringView("xyzxyz").find_last_not_of(std::string(buf), 4),
            CStringView::npos);
}

////////////////////////////////////////////////////////////////////////////////
// Non-member functions.

TEST(tl_cstring_view, operator_compare) {
  EXPECT_TRUE(CStringView("foo") == CStringView("foo"));
  EXPECT_FALSE(CStringView("foo") == CStringView("bar"));

  EXPECT_TRUE(CStringView("foo") == "foo");
  EXPECT_FALSE(CStringView("foo") == "bar");

  EXPECT_TRUE(CStringView("foo") == std::string("foo"));
  EXPECT_FALSE(CStringView("foo") == std::string("bar"));

  EXPECT_TRUE(CStringView("12") < CStringView("123"));
  EXPECT_FALSE(CStringView("12") > CStringView("123"));
  EXPECT_TRUE(CStringView("123") > CStringView("12"));
  EXPECT_FALSE(CStringView("123") < CStringView("12"));
  EXPECT_TRUE(CStringView("122") < CStringView("123"));
  EXPECT_FALSE(CStringView("122") > CStringView("123"));
  EXPECT_TRUE(CStringView("123") > CStringView("122"));
  EXPECT_FALSE(CStringView("123") < CStringView("122"));

  EXPECT_TRUE(CStringView("12") < "123");
  EXPECT_FALSE(CStringView("12") > "123");
  EXPECT_TRUE(CStringView("123") > "12");
  EXPECT_FALSE(CStringView("123") < "12");
  EXPECT_TRUE(CStringView("122") < "123");
  EXPECT_FALSE(CStringView("122") > "123");
  EXPECT_TRUE(CStringView("123") > "122");
  EXPECT_FALSE(CStringView("123") < "122");
}

TEST(tl_cstring_view, swap_non_member) {
  {
    CStringView a("012345");
    CStringView b("abcd");

    swap(a, b);

    EXPECT_EQ(a, "abcd");
    EXPECT_EQ(b, "012345");
  }
}

TEST(tl_cstring_view, PutToStream) {
  std::stringstream os;
  os << CStringView("Hello, World!");
  EXPECT_EQ(os.str(), "Hello, World!");
}

////////////////////////////////////////////////////////////////////////////////
// Maintenance.

// Avoid accident of a reference test committed.
TEST(tl_cstring_view, IsReal) { EXPECT_EQ(USE_STD_STRING_VIEW_REFERENCE, 0); }

}  // namespace test

}  // namespace tiny_lib::cstring_view
