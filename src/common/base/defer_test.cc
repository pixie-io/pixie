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
#include <random>

#include "src/common/base/defer.h"

namespace px {

TEST(DeferTest, Basic) {
  std::default_random_engine rng;
  std::uniform_int_distribution<int> dist(0, 1);

  for (uint32_t i = 0; i < 100; ++i) {
    int x;
    {
      x = 10;
      DEFER(x = i;);

      if (dist(rng) == 0) {
        x = 11;
      } else {
        x = 12;
      }
    }
    EXPECT_EQ(x, i);
  }
}

}  // namespace px
