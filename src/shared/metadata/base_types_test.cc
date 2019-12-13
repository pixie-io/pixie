#include <gtest/gtest.h>

#include <absl/hash/hash_testing.h>
#include "src/shared/metadata/base_types.h"

namespace pl {
namespace md {

TEST(UPID, check_upid_components) {
  auto upid = UPID(123, 456, 3420030816657ULL);
  EXPECT_EQ(123, upid.asid());
  EXPECT_EQ(456, upid.pid());
  EXPECT_EQ(3420030816657ULL, upid.start_ts());
}

TEST(UPID, check_upid_eq) {
  EXPECT_NE(UPID(12, 456, 3420030816657ULL), UPID(123, 456, 3420030816657ULL));
  EXPECT_NE(UPID(123, 456, 3420030816657ULL), UPID(123, 456, 3000000000000ULL));
  EXPECT_NE(UPID(123, 45, 3420030816657ULL), UPID(123, 456, 3420030816657ULL));

  EXPECT_EQ(UPID(123, 456, 3420030816657ULL), UPID(123, 456, 3420030816657ULL));
}

TEST(UPID, hash_func) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      UPID(123, 456, 789),
      UPID(13, 46, 3420030816657ULL),
      UPID(12, 456, 3420030816657ULL),
  }));
}

TEST(UPID, string) {
  EXPECT_EQ("123:456:3420030816657", UPID(123, 456, 3420030816657ULL).String());
  EXPECT_EQ("12:456:3420030816657", UPID(12, 456, 3420030816657ULL).String());
  EXPECT_EQ("12:46:3420030816657", UPID(12, 46, 3420030816657ULL).String());
}

}  // namespace md
}  // namespace pl
