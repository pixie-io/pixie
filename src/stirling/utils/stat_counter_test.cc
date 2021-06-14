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

#include "src/stirling/utils/stat_counter.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace utils {

using ::testing::StrEq;

enum class Key {
  kFirst,
  kSecond,
};

// Tests that StateCounter tracked values are correct; and printed string is correct as well.
TEST(StatCounterTest, CheckValuesAndPrint) {
  StatCounter<Key> counter;

  counter.Increment(Key::kFirst);
  EXPECT_EQ(counter.Get(Key::kFirst), 1);

  counter.Increment(Key::kFirst, 2);
  EXPECT_EQ(counter.Get(Key::kFirst), 3);

  counter.Decrement(Key::kFirst);
  EXPECT_EQ(counter.Get(Key::kFirst), 2);

  counter.Decrement(Key::kFirst, 2);
  EXPECT_EQ(counter.Get(Key::kFirst), 0);

  counter.Decrement(Key::kFirst);
  EXPECT_EQ(counter.Get(Key::kFirst), -1);

  EXPECT_THAT(counter.Print(), StrEq("kFirst=-1 kSecond=0 "));
}

}  // namespace utils
}  // namespace stirling
}  // namespace px
