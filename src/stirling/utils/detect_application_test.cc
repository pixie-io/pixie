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

#include "src/stirling/utils/detect_application.h"

#include <string>
#include <vector>

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {

using ::px::testing::status::StatusIs;

TEST(DetectApplicationTest, ResultsAreAsExpected) {
  EXPECT_EQ(Application::kUnknown, DetectApplication("/usr/bin/test"));
  EXPECT_EQ(Application::kNode, DetectApplication("/usr/bin/node"));
  EXPECT_EQ(Application::kNode, DetectApplication("/usr/bin/nodejs"));
}

TEST(GetSemVer, AsExpected) {
  ASSERT_OK_AND_ASSIGN(SemVer sem_ver, GetSemVer("v1.2.3-test"));
  EXPECT_EQ(sem_ver.major, 1);
  EXPECT_EQ(sem_ver.minor, 2);
  EXPECT_EQ(sem_ver.patch, 3);
  EXPECT_THAT(GetSemVer("v1.2.test").status(),
              StatusIs(px::statuspb::INVALID_ARGUMENT,
                       "Input 'v1.2.test' does not contain a semantic version number"));
}

}  // namespace stirling
}  // namespace px
