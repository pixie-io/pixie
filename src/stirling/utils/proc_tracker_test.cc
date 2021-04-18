/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/stirling/utils/proc_tracker.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace px {
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
}  // namespace px
