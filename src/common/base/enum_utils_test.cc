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

#include "src/common/base/enum_utils.h"
#include "src/common/testing/testing.h"

using ::testing::ElementsAre;
using ::testing::Pair;

enum class Color { kRed = 2, kBlue = 4, kGreen = 8 };

TEST(EnumUtils, EnumDef) {
  std::map<int64_t, std::string_view> enum_def = px::EnumDefToMap<Color>();

  EXPECT_THAT(enum_def, ElementsAre(Pair(2, "kRed"), Pair(4, "kBlue"), Pair(8, "kGreen")));
}
