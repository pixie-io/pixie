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

#include "src/common/testing/testing.h"
#include "src/stirling/core/data_table.h"
#include "src/stirling/core/types.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

using ::px::stirling::testing::RecordBatchSizeIs;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::EndsWith;
using ::testing::IsEmpty;
using ::testing::StartsWith;

static constexpr DataElement kElements[] = {
    {"a", "", types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
    {"b", "", types::DataType::STRING, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
    {"c", "", types::DataType::STRING, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
};
static constexpr auto kTableSchema =
    DataTableSchema("abc_table", "A table with A, B and C", kElements);

TEST(RecordBuilder, StringMaxSize) {
  DataTable data_table(/*id*/ 0, kTableSchema);

  constexpr size_t kMaxStringBytes = 512;

  std::string kLargeString(kMaxStringBytes + 100, 'c');
  std::string kExpectedString(kMaxStringBytes, 'c');

  DataTable::RecordBuilder<&kTableSchema> r(&data_table);
  r.Append<r.ColIndex("a")>(1, kMaxStringBytes);
  r.Append<r.ColIndex("b")>("foo", kMaxStringBytes);
  r.Append<r.ColIndex("c")>(kLargeString, kMaxStringBytes);

  std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
  ASSERT_EQ(tablets.size(), 1);
  types::ColumnWrapperRecordBatch& record_batch = tablets[0].records;

  ASSERT_THAT(record_batch, RecordBatchSizeIs(1));

  EXPECT_THAT(record_batch[2]->Get<types::StringValue>(0), StartsWith(kExpectedString));
  EXPECT_THAT(record_batch[2]->Get<types::StringValue>(0), EndsWith("[TRUNCATED]"));
}

TEST(RecordBuilder, MissingColumn) {
  DataTable data_table(/*id*/ 0, kTableSchema);

  auto r_ptr = std::make_unique<DataTable::RecordBuilder<&kTableSchema>>(&data_table);
  r_ptr->Append<0>(types::Int64Value(1));
  r_ptr->Append<2>(types::StringValue("bar"));
  EXPECT_DEBUG_DEATH(r_ptr.reset(), "");

  // Tricky: This is required because EXPECT_DEBUG_DEATH acts like a fork(),
  // which means the main process will continue on, and will trigger the
  // destructor a second time, which will cause the DCHECK to fire without this statement.
#if DCHECK_IS_ON()
  r_ptr->Append<1>(types::StringValue("foo"));
#endif
}

TEST(RecordBuilder, Duplicate) {
  DataTable data_table(/*id*/ 0, kTableSchema);

  auto r_ptr = std::make_unique<DataTable::RecordBuilder<&kTableSchema>>(&data_table);
  r_ptr->Append<0>(types::Int64Value(1));
  r_ptr->Append<1>(types::StringValue("foo"));
  r_ptr->Append<2>(types::StringValue("bar"));
  EXPECT_DEBUG_DEATH(r_ptr->Append<0>(types::Int64Value(1)), "");
}

TEST(RecordBuilder, UnfilledColNames) {
  DataTable data_table(/*id*/ 0, kTableSchema);

  DataTable::RecordBuilder<&kTableSchema> r(&data_table);
  EXPECT_THAT(r.UnfilledColNames(), ElementsAre("a", "b", "c"));

  r.Append<r.ColIndex("a")>(1);
  EXPECT_THAT(r.UnfilledColNames(), ElementsAre("b", "c"));
  r.Append<r.ColIndex("b")>("test");
  r.Append<r.ColIndex("c")>("test");
  EXPECT_THAT(r.UnfilledColNames(), IsEmpty());
}

TEST(DynamicRecordBuilder, StringMaxSize) {
  DataTable data_table(/*id*/ 0, kTableSchema);

  constexpr size_t kMaxStringBytes = 512;

  std::string kLargeString(kMaxStringBytes + 100, 'c');
  std::string kExpectedString(kMaxStringBytes, 'c');

  DataTable::DynamicRecordBuilder r(&data_table);
  r.Append<types::Int64Value>(0, 1, kMaxStringBytes);
  r.Append<types::StringValue>(1, "foo", kMaxStringBytes);
  r.Append<types::StringValue>(2, kLargeString, kMaxStringBytes);

  std::vector<TaggedRecordBatch> tablets = data_table.ConsumeRecords();
  ASSERT_EQ(tablets.size(), 1);
  types::ColumnWrapperRecordBatch& record_batch = tablets[0].records;

  ASSERT_THAT(record_batch, RecordBatchSizeIs(1));

  EXPECT_THAT(record_batch[2]->Get<types::StringValue>(0), StartsWith(kExpectedString));
  EXPECT_THAT(record_batch[2]->Get<types::StringValue>(0), EndsWith("[TRUNCATED]"));
}

TEST(DynamicRecordBuilder, MissingColumn) {
  DataTable data_table(/*id*/ 0, kTableSchema);

  auto r_ptr = std::make_unique<DataTable::DynamicRecordBuilder>(&data_table);
  r_ptr->Append(0, types::Int64Value(1));
  r_ptr->Append(2, types::StringValue("bar"));
  EXPECT_DEBUG_DEATH(r_ptr.reset(), "");

  // Tricky: This is required because EXPECT_DEBUG_DEATH acts like a fork(),
  // which means the main process will continue on, and will trigger the
  // destructor a second time, which will cause the DCHECK to fire without this statement.
#if DCHECK_IS_ON()
  r_ptr->Append(1, types::StringValue("foo"));
#endif
}

TEST(DynamicRecordBuilder, Duplicate) {
  DataTable data_table(/*id*/ 0, kTableSchema);

  auto r_ptr = std::make_unique<DataTable::DynamicRecordBuilder>(&data_table);
  r_ptr->Append(0, types::Int64Value(1));
  r_ptr->Append(1, types::StringValue("foo"));
  r_ptr->Append(2, types::StringValue("bar"));
  EXPECT_DEBUG_DEATH(r_ptr->Append(0, types::Int64Value(1)), "");
}

}  // namespace stirling
}  // namespace px
