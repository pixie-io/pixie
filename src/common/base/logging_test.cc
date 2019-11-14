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
#if DCHECK_IS_ON()
  EXPECT_DEATH((ECHECK(false)), "");

  EXPECT_DEATH(ECHECK_EQ(1, 2), "");
  EXPECT_DEATH(ECHECK_EQ(2, 1), "");

  EXPECT_DEATH(ECHECK_NE(1, 1), "");

  EXPECT_DEATH(ECHECK_LE(2, 1), "");

  EXPECT_DEATH(ECHECK_LT(1, 1), "");
  EXPECT_DEATH(ECHECK_LT(2, 1), "");

  EXPECT_DEATH(ECHECK_GE(1, 2), "");

  EXPECT_DEATH(ECHECK_GT(1, 1), "");
  EXPECT_DEATH(ECHECK_GT(1, 2), "");
#else
  ECHECK(false);

  ECHECK_EQ(1, 2);
  ECHECK_EQ(2, 1);

  ECHECK_NE(1, 1);

  ECHECK_LE(2, 1);

  ECHECK_LT(1, 1);
  ECHECK_LT(2, 1);

  ECHECK_GE(1, 2);

  ECHECK_GT(1, 1);
  ECHECK_GT(1, 2);
#endif
}

}  // namespace pl
