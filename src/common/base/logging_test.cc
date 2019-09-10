#include <gtest/gtest.h>

#include "src/common/base/logging.h"

namespace pl {

TEST(ECheckTest, check_true) { ECHECK(true); }

TEST(ECheckTest, check_false) {
// Behavior changes based on build type.
// Relying on build system (Jenkins) to stress both cases.
#if DCHECK_IS_ON()
  EXPECT_DEATH(ECHECK(false), "");
#else
  ECHECK(false);
#endif
}

}  // namespace pl
