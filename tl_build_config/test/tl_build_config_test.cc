// Copyright (c) 2021 tiny lib authors
//
// SPDX-License-Identifier: MIT-0

#include "tl_build_config.h"

#include <bit>
#include <cstdint>

#include "tiny_lib/unittest/test.h"

// static_assert(_BUILD_CONFIG_CAN_USE(FLAG_UNKNOWN) == 0);

#define FLAG_EMPTY
#define FLAG_ZERO 0
#define FLAG_ONE 1

static_assert(_TL_BUILD_CONFIG_CAN_USE(FLAG_EMPTY));
static_assert(!_TL_BUILD_CONFIG_CAN_USE(FLAG_ZERO));
static_assert(_TL_BUILD_CONFIG_CAN_USE(FLAG_ONE));

namespace tl {

TEST(BuildConfig, Endian) {
#if ARCH_CPU_BIG_ENDIAN
  EXPECT_EQ(std::endian::native, std::endian::big);
#elif ARCH_CPU_LITTLE_ENDIAN
  EXPECT_EQ(std::endian::native, std::endian::little);
#else
  ADD_FAILURE() << "Unhandled endian check.";
#endif
}

}  // namespace tl
