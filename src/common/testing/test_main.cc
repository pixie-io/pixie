#include <gtest/gtest.h>
#include "src/common/base/base.h"

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  px::EnvironmentGuard env_guard(&argc, argv);
  int retval = RUN_ALL_TESTS();
  return retval;
}
