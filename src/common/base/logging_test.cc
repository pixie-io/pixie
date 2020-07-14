#include "src/common/base/logging.h"
#include "src/common/testing/testing.h"

namespace pl {

TEST(ECheckTest, check_true) {
  ECHECK(true);

  ECHECK_EQ(1, 1);

  ECHECK_NE(1, 2);
  ECHECK_NE(2, 1);

  ECHECK_LE(1, 1);
  ECHECK_LE(1, 2);

  ECHECK_LT(1, 2);

  ECHECK_GE(1, 1);
  ECHECK_GE(2, 1);

  ECHECK_GT(2, 1);
}

TEST(ECheckTest, check_false) {
  // Behavior changes based on build type.
  // Rely on build system (Jenkins) to stress both cases.
  EXPECT_DEBUG_DEATH((ECHECK(false)), "");

  EXPECT_DEBUG_DEATH(ECHECK_EQ(1, 2), "");
  EXPECT_DEBUG_DEATH(ECHECK_EQ(2, 1), "");

  EXPECT_DEBUG_DEATH(ECHECK_NE(1, 1), "");

  EXPECT_DEBUG_DEATH(ECHECK_LE(2, 1), "");

  EXPECT_DEBUG_DEATH(ECHECK_LT(1, 1), "");
  EXPECT_DEBUG_DEATH(ECHECK_LT(2, 1), "");

  EXPECT_DEBUG_DEATH(ECHECK_GE(1, 2), "");

  EXPECT_DEBUG_DEATH(ECHECK_GT(1, 1), "");
  EXPECT_DEBUG_DEATH(ECHECK_GT(1, 2), "");
}

}  // namespace pl
