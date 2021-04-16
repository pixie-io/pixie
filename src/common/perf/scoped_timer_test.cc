#include <gtest/gtest.h>
#include <chrono>
#include <thread>

#include <absl/strings/match.h>
#include "src/common/perf/scoped_timer.h"

namespace px {

TEST(scoped_timer, time_basic) {
  FLAGS_alsologtostderr = true;
  testing::internal::CaptureStderr();
  {
    px::ScopedTimer val("test");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  std::string output = testing::internal::GetCapturedStderr();
  EXPECT_TRUE(absl::StrContains(output, "Timer(test)"));
}

}  // namespace px
