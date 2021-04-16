#include <gtest/gtest.h>

#include "src/common/base/hash_utils.h"

namespace px {

TEST(HashUtils, HashCombine) { EXPECT_EQ(0xf4ff80ec63c103d4ULL, HashCombine(0, 1)); }

}  // namespace px
