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

#include "src/shared/stats/metrics.h"

namespace pl {
namespace shared {
namespace stats {

TEST(Counter, no_tags) {
  auto counter = MakeCounter("test_counter", {});
  EXPECT_EQ(counter->Name(), "test_counter");
  EXPECT_EQ(counter->Tags().size(), 0);

  EXPECT_EQ(counter->Value(), 0);
  counter->Inc();
  EXPECT_EQ(counter->Value(), 1);
  counter->Add(9);
  EXPECT_EQ(counter->Value(), 10);
}

TEST(Counter, tags) {
  auto counter = MakeCounter("test_counter", {Tag("val", "1"), Tag("val2", "2")});
  EXPECT_EQ(counter->Name(), "test_counter");
  EXPECT_EQ(counter->Tags().size(), 2);
  EXPECT_EQ(counter->Tags()[0].name(), "val");
  EXPECT_EQ(counter->Tags()[0].value(), "1");
}

}  // namespace stats
}  // namespace shared
}  // namespace pl
