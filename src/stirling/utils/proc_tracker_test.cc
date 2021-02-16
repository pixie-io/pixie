#include "src/stirling/utils/proc_tracker.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace pl {
namespace stirling {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

class ProcTrackerTest : public ::testing::Test {
 protected:
  ProcTracker proc_tracker_;
};

TEST_F(ProcTrackerTest, Basic) {
  using UPIDSet = absl::flat_hash_set<md::UPID>;

  const md::UPID kUPID1 = md::UPID(0, 1, 111);
  const md::UPID kUPID2 = md::UPID(0, 2, 222);
  const md::UPID kUPID3 = md::UPID(0, 3, 333);
  const md::UPID kUPID4 = md::UPID(0, 4, 444);

  proc_tracker_.Update(UPIDSet{kUPID1, kUPID2});
  EXPECT_THAT(proc_tracker_.upids(), UnorderedElementsAre(kUPID1, kUPID2));
  EXPECT_THAT(proc_tracker_.new_upids(), UnorderedElementsAre(kUPID1, kUPID2));
  EXPECT_THAT(proc_tracker_.deleted_upids(), IsEmpty());

  proc_tracker_.Update(UPIDSet{kUPID1, kUPID2, kUPID3});
  EXPECT_THAT(proc_tracker_.upids(), UnorderedElementsAre(kUPID1, kUPID2, kUPID3));
  EXPECT_THAT(proc_tracker_.new_upids(), UnorderedElementsAre(kUPID3));
  EXPECT_THAT(proc_tracker_.deleted_upids(), IsEmpty());

  proc_tracker_.Update(UPIDSet{kUPID1, kUPID3});
  EXPECT_THAT(proc_tracker_.upids(), UnorderedElementsAre(kUPID1, kUPID3));
  EXPECT_THAT(proc_tracker_.new_upids(), IsEmpty());
  EXPECT_THAT(proc_tracker_.deleted_upids(), UnorderedElementsAre(kUPID2));

  proc_tracker_.Update(UPIDSet{kUPID1, kUPID4});
  EXPECT_THAT(proc_tracker_.upids(), UnorderedElementsAre(kUPID1, kUPID4));
  EXPECT_THAT(proc_tracker_.new_upids(), UnorderedElementsAre(kUPID4));
  EXPECT_THAT(proc_tracker_.deleted_upids(), UnorderedElementsAre(kUPID3));
}

}  // namespace stirling
}  // namespace pl
