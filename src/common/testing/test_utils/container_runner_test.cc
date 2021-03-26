#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/test_utils/test_container.h"
#include "src/common/testing/testing.h"

namespace pl {

TEST(ContainerRunnerTest, Run) {
  SleepContainer container;
  ASSERT_OK(container.Run());
}

}  // namespace pl
