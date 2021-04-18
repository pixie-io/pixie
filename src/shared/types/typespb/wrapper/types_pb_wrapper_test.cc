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

#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"

TEST(SemanticType, MagicEnumBehavior) {
  // A semantic type with a low numeric value should always resolve.
  EXPECT_EQ(magic_enum::enum_name(px::types::ST_NONE), "ST_NONE");

  // Make sure the magic_enum::customize is kicking in,
  // which increases the max enum value that is printable.
  EXPECT_EQ(magic_enum::enum_name(px::types::ST_HTTP_RESP_MESSAGE), "ST_HTTP_RESP_MESSAGE");

  // Make sure that calling enum_name on a large semantic type does not cause any issues
  // other than not printing out the name.
  EXPECT_EQ(magic_enum::enum_name(px::types::SemanticType_INT_MAX_SENTINEL_DO_NOT_USE_), "");
}
