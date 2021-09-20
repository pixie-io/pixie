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

#include "src/common/testing/testing.h"

#include "src/common/clock/clock_conversion.h"

namespace px {
namespace clock {

TEST(InterpolatingLookupTable, basic) {
  InterpolatingLookupTable<64> table;

  table.Emplace(1, 1);
  table.Emplace(3, 5);
  table.Emplace(5, 9);
  ASSERT_EQ(3, table.size());

  EXPECT_EQ(1, table.Get(1));
  EXPECT_EQ(5, table.Get(3));
  EXPECT_EQ(9, table.Get(5));

  EXPECT_EQ(3, table.Get(2));
  EXPECT_EQ(7, table.Get(4));

  // Since 0 is outside the table, we use the first offset (which is also 0)
  EXPECT_EQ(0, table.Get(0));
  // Since 6 is outside the table we use the last offset (which is 4).
  EXPECT_EQ(6 + 4, table.Get(6));
}

// This test makes sure that the InterpolatingLookupTable can handle non-monotonic points, eg.
// points where x has increased from the previous x but the y value has decreased from the previous
// y value.
TEST(InterpolatingLookupTable, nonmonotonic) {
  InterpolatingLookupTable<64> table;

  table.Emplace(0, 0);
  table.Emplace(150, 200);
  table.Emplace(250, 350);
  table.Emplace(300, 300);

  EXPECT_EQ(0, table.Get(0));
  EXPECT_EQ(200, table.Get(150));
  EXPECT_EQ(350, table.Get(250));
  EXPECT_EQ(300, table.Get(300));

  EXPECT_EQ(67, table.Get(50));
  EXPECT_EQ(133, table.Get(100));
  EXPECT_EQ(275, table.Get(200));
  EXPECT_EQ(350, table.Get(350));
}

TEST(InterpolatingLookupTable, mono_and_realtime) {
  InterpolatingLookupTable<64> table;
  uint64_t start_mono = 1000000000000000;
  uint64_t end_mono = 1000200000000000;
  uint64_t offset = 1633546462397086812;
  table.Emplace(start_mono, start_mono + offset);
  table.Emplace(end_mono, end_mono + offset);

  uint64_t query;

  query = start_mono + 10000000000;
  EXPECT_EQ(offset, table.Get(query) - query);

  query = start_mono - 100000000;
  EXPECT_EQ(offset, table.Get(query) - query);

  query = end_mono + 1000000000;
  EXPECT_EQ(offset, table.Get(query) - query);
}

}  // namespace clock
}  // namespace px
