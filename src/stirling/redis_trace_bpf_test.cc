#include "src/common/exec/exec.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/common/testing/testing.h"
#include "src/stirling/testing/testing.h"

namespace pl {
namespace stirling {

using ::testing::StrEq;

class RedisContainer : public ContainerRunner {
 public:
  RedisContainer() : ContainerRunner(kImageName, kContainerNamePrefix, kReadyMessage) {}

 private:
  static constexpr std::string_view kImageName = "redis";
  static constexpr std::string_view kContainerNamePrefix = "redis_test";
  static constexpr std::string_view kReadyMessage = "# Server initialized";
};

class RedisTraceBPFTest : public testing::SocketTraceBPFTest</* TClientSideTracing */ false> {
 protected:
  RedisTraceBPFTest() { PL_CHECK_OK(container_.Run(150, {})); }

  RedisContainer container_;
};

// A place holder to verify the setup of the redis and redis_cli container.
TEST_F(RedisTraceBPFTest, Dummy) {
  const std::string redis_cli_cmd =
      absl::Substitute("docker run --rm --network=container:$0 redis redis-cli flushall",
                       container_.container_name());
  PL_LOG_VAR(redis_cli_cmd);
  ASSERT_OK_AND_ASSIGN(const std::string output, pl::Exec(redis_cli_cmd));
  EXPECT_THAT(output, StrEq("OK\n"));
}

}  // namespace stirling
}  // namespace pl
