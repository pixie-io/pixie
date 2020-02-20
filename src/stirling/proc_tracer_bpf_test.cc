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

  std::vector<proc_creation_event_t> events;
  for (const auto& event : proc_tracer.ExtractProcCreationEvents()) {
    if (event.ppid == ppid) {
      events.push_back(event);
    }
  }
  ASSERT_THAT(events, SizeIs(2));
  EXPECT_EQ(pid1, events[0].pid);
  EXPECT_EQ(pid2, events[1].pid);
}

}  // namespace stirling
}  // namespace pl
