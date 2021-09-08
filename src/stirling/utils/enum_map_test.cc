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

#include "src/stirling/utils/enum_map.h"

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace utils {

enum class KeyEnum {
  kFoo = 0,
  kBar = 10,
};

struct Value {
  bool value;
};

TEST(EnumMapTest, SetGetValue) {
  EnumMap<KeyEnum, Value> map;

  EXPECT_TRUE(map.Set(KeyEnum::kFoo, {false}));
  EXPECT_FALSE(map.Get(KeyEnum::kFoo).value);
  EXPECT_FALSE(map.AreAllKeysSet());

  EXPECT_TRUE(map.Set(KeyEnum::kBar, {true}));
  EXPECT_TRUE(map.Get(KeyEnum::kBar).value);
  EXPECT_TRUE(map.AreAllKeysSet());
}

}  // namespace utils
}  // namespace stirling
}  // namespace px
