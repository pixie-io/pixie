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

#include "src/stirling/core/output.h"

#include "src/common/testing/testing.h"
#include "src/stirling/testing/test_table.h"

namespace px {
namespace stirling {

using ::px::stirling::testing::TestTableFixture;
using ::testing::ElementsAre;
using ::testing::StrEq;

namespace idx = ::px::stirling::testing::test_table_idx;

std::unique_ptr<TestTableFixture> GetTestTableFixture() {
  auto fixture = std::make_unique<TestTableFixture>();
  {
    auto r = fixture->record_builder();
    r.Append<idx::kInt64Idx>(0);
    r.Append<idx::kStringIdx>("test");
  }
  {
    auto r = fixture->record_builder();
    r.Append<idx::kInt64Idx>(0);
    r.Append<idx::kStringIdx>("test");
  }
  return fixture;
}

TEST(PrintRecordBatchTest, AllRecordsToStringPrefixWrapper) {
  auto fixture = GetTestTableFixture();
  EXPECT_EQ(
      "[test] int64:[0] string:[test]\n"
      "[test] int64:[0] string:[test]\n",
      ToString("test", fixture->SchemaProto(), fixture->record_batch()));
}

TEST(PrintRecordBatchTest, AllRecordsToStringNonPrefixWrapper) {
  auto fixture = GetTestTableFixture();
  EXPECT_THAT(ToString(testing::kTestTable.ToProto(), fixture->record_batch()),
              ElementsAre(StrEq(" int64:[0] string:[test]"), StrEq(" int64:[0] string:[test]")));
}

}  // namespace stirling
}  // namespace px
