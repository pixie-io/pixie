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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "src/common/system/uid.h"
#include "src/common/testing/testing.h"

namespace px {
namespace system {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;

TEST(NameForUIDTest, NameIsNotEmpty) { EXPECT_OK_AND_THAT(NameForUID(getuid()), Not(IsEmpty())); }

TEST(ParsePasswdTest, ResultsAreAsExpected) {
  const char kSamplePasswd[] =
      "root:x:0:3:root:/root:/bin/bash\n"
      "daemon:x:1:4:daemon:/usr/sbin:/usr/sbin/nologin\n"
      "bin:x:2:5:bin:/bin:/usr/sbin/nologin";
  EXPECT_THAT(ParsePasswd(kSamplePasswd),
              ElementsAre(Pair(0, "root"), Pair(1, "daemon"), Pair(2, "bin")));
}

}  // namespace system
}  // namespace px
