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

#include "src/common/fs/inode_utils.h"
#include "src/common/testing/testing.h"

namespace px {
namespace fs {

TEST(InodeUtils, ExtractInodeNum) {
  EXPECT_OK_AND_EQ(ExtractInodeNum(kSocketInodePrefix, "socket:[32431]"), 32431);
  EXPECT_OK_AND_EQ(ExtractInodeNum(kNetInodePrefix, "net:[1234]"), 1234);

  // Inodes are 32-bit unsigned numbers, so check for that too.
  EXPECT_OK_AND_EQ(ExtractInodeNum(kNetInodePrefix, "net:[3221225472]"), 3221225472);

  EXPECT_NOT_OK(ExtractInodeNum(kSocketInodePrefix, "socket:[32x31]"));
  EXPECT_NOT_OK(ExtractInodeNum(kSocketInodePrefix, "socket[32431]"));
  EXPECT_NOT_OK(ExtractInodeNum(kNetInodePrefix, "[32431]"));
  EXPECT_NOT_OK(ExtractInodeNum(kNetInodePrefix, "socket:[32431]"));
}

}  // namespace fs
}  // namespace px
