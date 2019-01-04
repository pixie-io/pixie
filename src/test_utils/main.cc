#include <gtest/gtest.h>
#include "src/common/env.h"

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  pl::InitEnvironmentOrDie(&argc, argv);
  int retval = RUN_ALL_TESTS();
  pl::ShutdownEnvironmentOrDie();
  return retval;
}
