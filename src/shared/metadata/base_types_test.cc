#include <gtest/gtest.h>

#include "absl/hash/hash_testing.h"
#include "src/shared/metadata/base_types.h"

namespace pl {
namespace md {

TEST(UPID, check_upid_components) {
  auto upid = UPID(123, 456, 78910);
  EXPECT_EQ(123, upid.agent_id());
  EXPECT_EQ(456, upid.pid());
  EXPECT_EQ(78910ULL, upid.start_ts());
}

TEST(UPID, check_upid_eq) {
  EXPECT_NE(UPID(12, 456, 78910), UPID(123, 456, 78910));
  EXPECT_NE(UPID(123, 456, 7891), UPID(123, 456, 78910));
  EXPECT_NE(UPID(123, 45, 7891), UPID(123, 456, 7891));

  EXPECT_EQ(UPID(123, 456, 78910), UPID(123, 456, 78910));
}

TEST(UPID, hash_func) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      UPID(123, 456, 78910),
      UPID(13, 46, 7910),
      UPID(12, 456, 7910),
  }));
}

TEST(UPID, string) {
  EXPECT_EQ("123:456:78910", UPID(123, 456, 78910).String());
  EXPECT_EQ("12:456:78910", UPID(12, 456, 78910).String());
  EXPECT_EQ("12:46:78910", UPID(12, 46, 78910).String());
}

}  // namespace md
}  // namespace pl
