#include "src/stirling/obj_tools/obj_tools.h"

#include "src/common/system/proc_parser.h"
#include "src/common/testing/test_environment.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {
namespace obj_tools {

using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::EndsWith;
using ::testing::IsEmpty;
using ::testing::Pair;

// Tests GetActiveBinaries() resolves this running test itself.
// We instruct GetActiveBinaries() to behave as if the test is not running inside a container.
TEST(GetActiveBinariesTest, CaptureTestBinary) {
  int32_t mypid = getpid();
  std::map<int32_t, std::filesystem::path> pid_paths = {
      {mypid, std::filesystem::path("/proc") / std::to_string(mypid)}};
  const std::map<std::string, std::vector<int>> binaries = GetActiveBinaries(pid_paths);
  EXPECT_THAT(binaries, Contains(Pair(EndsWith("src/stirling/obj_tools/obj_tools_test"), _)))
      << "Should see the test process itself";
}

TEST(GetActiveBinaryTest, CaptureTestBinaryByPath) {
  std::filesystem::path pid_path = std::filesystem::path("/proc") / std::to_string(getpid());
  EXPECT_OK_AND_THAT(GetActiveBinary(/*host_path*/ {}, pid_path),
                     EndsWith("src/stirling/obj_tools/obj_tools_test"));
}

TEST(GetActiveBinaryTest, CaptureTestBinaryByPID) {
  std::filesystem::path pid_path = std::filesystem::path("/proc") / std::to_string(getpid());
  EXPECT_OK_AND_THAT(GetActiveBinary(getpid()), EndsWith("src/stirling/obj_tools/obj_tools_test"));

  int64_t start_time = system::GetPIDStartTimeTicks(pid_path);
  EXPECT_OK_AND_THAT(GetActiveBinary(getpid(), start_time),
                     EndsWith("src/stirling/obj_tools/obj_tools_test"));
  EXPECT_NOT_OK(GetActiveBinary(getpid(), start_time + 1));
}

}  // namespace obj_tools
}  // namespace stirling
}  // namespace pl
