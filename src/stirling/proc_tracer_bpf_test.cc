#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <absl/strings/numbers.h>

#include "src/common/base/test_utils.h"
#include "src/common/exec/exec.h"
#include "src/common/testing/test_environment.h"
#include "src/stirling/proc_tracer.h"

namespace pl {
namespace stirling {

using ::pl::Exec;
using ::pl::TestEnvironment;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

TEST(ProcTracerTest, CapturesVforkAndClone) {
  ProcTracer proc_tracer;
  ASSERT_OK(proc_tracer.Init());
  std::string path = TestEnvironment::PathToTestDataFile("src/stirling/testing/forker");
  ASSERT_OK_AND_ASSIGN(std::string output, Exec(path));

  std::vector<std::string_view> pids = absl::StrSplit(output, " ");
  ASSERT_THAT(pids, SizeIs(3));

  uint32_t ppid = 0;
  uint32_t pid1 = 0;
  uint32_t pid2 = 0;

  ASSERT_TRUE(absl::SimpleAtoi(pids[0], &ppid));
  ASSERT_TRUE(absl::SimpleAtoi(pids[1], &pid1));
  ASSERT_TRUE(absl::SimpleAtoi(pids[2], &pid2));

  // Retrieve the PIDs of the forked child processes. And check with the value obtained from the
  // stdout.
  std::vector<uint32_t> child_pids;
  for (const auto& event : proc_tracer.ExtractProcCreationEvents()) {
    if (event.ppid == ppid) {
      child_pids.push_back(event.pid);
    }
  }
  EXPECT_THAT(child_pids, UnorderedElementsAre(pid1, pid2));
}

}  // namespace stirling
}  // namespace pl
