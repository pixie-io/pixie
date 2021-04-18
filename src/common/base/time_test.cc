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

#include <gtest/gtest.h>

#include "src/common/base/time.h"
#include "src/common/testing/testing.h"

namespace px {

TEST(StringToRange, basic) {
  ASSERT_OK_AND_ASSIGN(auto output_pair, StringToTimeRange("5,20"));
  EXPECT_EQ(5, output_pair.first);
  EXPECT_EQ(20, output_pair.second);
}

TEST(StringToRange, invalid_format) { ASSERT_NOT_OK(StringToTimeRange("hi")); }

TEST(StringToTime, basic) {
  EXPECT_OK_AND_EQ(StringToTimeInt("-2m"), -120000000000);
  EXPECT_OK_AND_EQ(StringToTimeInt("2m"), 120000000000);
  EXPECT_OK_AND_EQ(StringToTimeInt("-10s"), -10000000000);
  EXPECT_OK_AND_EQ(StringToTimeInt("10d"), 864000000000000);
  EXPECT_OK_AND_EQ(StringToTimeInt("5h"), 18000000000000);
  EXPECT_OK_AND_EQ(StringToTimeInt("10ms"), 10000000);
}

TEST(StringToTime, invalid_format) { EXPECT_FALSE(StringToTimeInt("hello").ok()); }

TEST(PrettyDuration, strings) {
  EXPECT_EQ("1.23 \u03BCs", PrettyDuration(1230));
  EXPECT_EQ("14.56 ms", PrettyDuration(14561230));
  EXPECT_EQ("14.56 s", PrettyDuration(14561230000));
}

}  // namespace px
