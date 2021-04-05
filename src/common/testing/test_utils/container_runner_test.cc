#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/test_utils/test_container.h"
#include "src/common/testing/testing.h"

namespace pl {

TEST(ContainerRunnerTest, Run) {
  SleepContainer container;
  constexpr int kTimeoutSeconds = 30;
  ASSERT_OK(container.Run(kTimeoutSeconds));
}

}  // namespace pl
