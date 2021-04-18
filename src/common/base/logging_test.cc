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

#include "src/common/base/logging.h"
#include "src/common/testing/testing.h"

namespace px {

TEST(ECheckTest, check_true) {
  ECHECK(true);

  ECHECK_EQ(1, 1);

  ECHECK_NE(1, 2);
  ECHECK_NE(2, 1);

  ECHECK_LE(1, 1);
  ECHECK_LE(1, 2);

  ECHECK_LT(1, 2);

  ECHECK_GE(1, 1);
  ECHECK_GE(2, 1);

  ECHECK_GT(2, 1);
}

TEST(ECheckTest, check_false) {
  // Behavior changes based on build type.
  // Rely on build system (Jenkins) to stress both cases.
  EXPECT_DEBUG_DEATH((ECHECK(false)), "");

  EXPECT_DEBUG_DEATH(ECHECK_EQ(1, 2), "");
  EXPECT_DEBUG_DEATH(ECHECK_EQ(2, 1), "");

  EXPECT_DEBUG_DEATH(ECHECK_NE(1, 1), "");

  EXPECT_DEBUG_DEATH(ECHECK_LE(2, 1), "");

  EXPECT_DEBUG_DEATH(ECHECK_LT(1, 1), "");
  EXPECT_DEBUG_DEATH(ECHECK_LT(2, 1), "");

  EXPECT_DEBUG_DEATH(ECHECK_GE(1, 2), "");

  EXPECT_DEBUG_DEATH(ECHECK_GT(1, 1), "");
  EXPECT_DEBUG_DEATH(ECHECK_GT(1, 2), "");
}

}  // namespace px
