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
  absl::flat_hash_set<md::UPID> pids = ProcTracker::ListUPIDs(proc_path);
  EXPECT_THAT(ProcTracker::ListUPIDs(proc_path),
              UnorderedElementsAre(md::UPID{0, 123, 14329}, md::UPID{0, 1, 0},
                                   md::UPID{0, 456, 17594622}, md::UPID{0, 789, 46120203}));
}

class ProcTrackerTest : public ::testing::Test {
 protected:
  ProcTracker proc_tracker_;
};

TEST_F(ProcTrackerTest, TakeSnapshotAndDiff) {
  EXPECT_THAT(proc_tracker_.TakeSnapshotAndDiff({{md::UPID(0, 1, 123)}, {md::UPID(0, 2, 456)}}),
              UnorderedElementsAre(md::UPID(0, 1, 123), md::UPID(0, 2, 456)));
  EXPECT_THAT(proc_tracker_.upids(),
              UnorderedElementsAre(md::UPID(0, 1, 123), md::UPID(0, 2, 456)));
  EXPECT_THAT(proc_tracker_.TakeSnapshotAndDiff({
                  {md::UPID(0, 1, 123)},
                  {md::UPID(0, 2, 456)},
                  {md::UPID(0, 3, 789)},
              }),
              UnorderedElementsAre(md::UPID(0, 3, 789)));
  EXPECT_THAT(proc_tracker_.upids(),
              UnorderedElementsAre(md::UPID(0, 1, 123), md::UPID(0, 2, 456), md::UPID(0, 3, 789)));
}

}  // namespace stirling
}  // namespace pl
