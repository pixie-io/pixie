#include "src/stirling/utils/proc_tracker.h"

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {

using ::pl::testing::TestFilePath;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(ProcTrackerListPIDsTest, ListUPIDs) {
  const std::filesystem::path proc_path = TestFilePath("src/common/system/testdata/proc");
  absl::flat_hash_map<md::UPID, std::filesystem::path> pids = ProcTracker::ListUPIDs(proc_path);
  EXPECT_THAT(ProcTracker::ListUPIDs(proc_path),
              UnorderedElementsAre(Pair(md::UPID{0, 123, 14329}, proc_path / "123"),
                                   Pair(md::UPID{0, 1, 0}, proc_path / "1"),
                                   Pair(md::UPID{0, 456, 17594622}, proc_path / "456"),
                                   Pair(md::UPID{0, 789, 46120203}, proc_path / "789")));
}

class ProcTrackerTest : public ::testing::Test {
 protected:
  ProcTracker proc_tracker_;
};

TEST_F(ProcTrackerTest, TakeSnapshotAndDiff) {
  EXPECT_THAT(proc_tracker_.TakeSnapshotAndDiff(
                  {{md::UPID(0, 1, 123), "/proc/1"}, {md::UPID(0, 2, 456), "/proc/2"}}),
              UnorderedElementsAre(Pair(md::UPID(0, 1, 123), "/proc/1"),
                                   Pair(md::UPID(0, 2, 456), "/proc/2")));
  EXPECT_THAT(proc_tracker_.upids(), UnorderedElementsAre(Pair(md::UPID(0, 1, 123), "/proc/1"),
                                                          Pair(md::UPID(0, 2, 456), "/proc/2")));
  EXPECT_THAT(proc_tracker_.TakeSnapshotAndDiff({
                  {md::UPID(0, 1, 123), "/proc/1"},
                  {md::UPID(0, 2, 456), "/proc/2"},
                  {md::UPID(0, 3, 789), "/proc/3"},
              }),
              UnorderedElementsAre(Pair(md::UPID(0, 3, 789), "/proc/3")));
  EXPECT_THAT(proc_tracker_.upids(), UnorderedElementsAre(Pair(md::UPID(0, 1, 123), "/proc/1"),
                                                          Pair(md::UPID(0, 2, 456), "/proc/2"),
                                                          Pair(md::UPID(0, 3, 789), "/proc/3")));
}

}  // namespace stirling
}  // namespace pl
