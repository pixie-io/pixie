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

#include <memory>
#include <string>
#include <vector>

#include "src/table_store/table/internal/store_with_row_accounting.h"
#include "src/table_store/table/internal/test_utils.h"

namespace px {
namespace table_store {
namespace internal {

class ColdStoreTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rel_ = std::make_unique<schema::Relation>(
        std::vector<types::DataType>{types::DataType::TIME64NS, types::DataType::BOOLEAN,
                                     types::DataType::STRING},
        std::vector<std::string>{"col0", "col1", "col2"});
    store_ = std::make_unique<StoreWithRowTimeAccounting<StoreType::Cold>>(*rel_, 0);
  }

  schema::RowBatch MakeRowBatch(const std::vector<types::Time64NSValue>& times,
                                const std::vector<types::BoolValue>& bools,
                                const std::vector<types::StringValue>& strings) {
    schema::RowBatch rb(schema::RowDescriptor(rel_->col_types()), times.size());
    PX_CHECK_OK(rb.AddColumn(types::ToArrow(times, arrow::default_memory_pool())));
    PX_CHECK_OK(rb.AddColumn(types::ToArrow(bools, arrow::default_memory_pool())));
    PX_CHECK_OK(rb.AddColumn(types::ToArrow(strings, arrow::default_memory_pool())));
    return rb;
  }

  std::unique_ptr<schema::Relation> rel_;
  std::unique_ptr<StoreWithRowTimeAccounting<StoreType::Cold>> store_;
};

class HotStoreTest : public RecordOrRowBatchParamTest {
 protected:
  void SetUp() override {
    RecordOrRowBatchParamTest::SetUp();
    store_ = std::make_unique<StoreWithRowTimeAccounting<StoreType::Hot>>(*rel_, 0);
  }
  std::unique_ptr<StoreWithRowTimeAccounting<StoreType::Hot>> store_;
};

TEST_F(ColdStoreTest, PushRowBatchesCheckProperties) {
  std::vector<types::Time64NSValue> times = {1, 1, 10, 11};
  std::vector<types::BoolValue> bools = {true, false, true, false};
  std::vector<types::StringValue> strings = {"ab", "cd", "ef", "gh"};
  auto rb0 = MakeRowBatch(times, bools, strings);

  EXPECT_EQ(0, store_->Size());

  store_->EmplaceBack(0, rb0.columns());
  auto next_row_id = 4;
  EXPECT_EQ(1, store_->Size());

  EXPECT_EQ(0, store_->FirstRowID());
  EXPECT_EQ(3, store_->LastRowID());

  auto optional_row_id = store_->FindRowIDFromTimeFirstGreaterThanOrEqual(1);
  ASSERT_TRUE(optional_row_id.has_value());
  EXPECT_EQ(0, optional_row_id.value());
  optional_row_id = store_->FindRowIDFromTimeFirstGreaterThanOrEqual(12);
  ASSERT_FALSE(optional_row_id.has_value());
  optional_row_id = store_->FindRowIDFromTimeFirstGreaterThan(1);
  ASSERT_TRUE(optional_row_id.has_value());
  EXPECT_EQ(2, optional_row_id.value());
  optional_row_id = store_->FindRowIDFromTimeFirstGreaterThan(11);
  ASSERT_FALSE(optional_row_id.has_value());

  EXPECT_EQ(1, store_->MinTime());

  times = {20, 20, 21};
  bools = {false, false, false};
  strings = {"", "", ""};
  auto rb1 = MakeRowBatch(times, bools, strings);

  store_->EmplaceBack(next_row_id, rb1.columns());
  EXPECT_EQ(2, store_->Size());

  EXPECT_EQ(0, store_->FirstRowID());
  EXPECT_EQ(6, store_->LastRowID());

  optional_row_id = store_->FindRowIDFromTimeFirstGreaterThanOrEqual(12);
  ASSERT_TRUE(optional_row_id.has_value());
  EXPECT_EQ(4, optional_row_id.value());
  optional_row_id = store_->FindRowIDFromTimeFirstGreaterThanOrEqual(22);
  ASSERT_FALSE(optional_row_id.has_value());
  optional_row_id = store_->FindRowIDFromTimeFirstGreaterThan(20);
  ASSERT_TRUE(optional_row_id.has_value());
  EXPECT_EQ(6, optional_row_id.value());
  optional_row_id = store_->FindRowIDFromTimeFirstGreaterThan(21);
  ASSERT_FALSE(optional_row_id.has_value());

  EXPECT_EQ(1, store_->MinTime());

  store_->PopFront();

  EXPECT_EQ(1, store_->Size());
  EXPECT_EQ(4, store_->FirstRowID());
  EXPECT_EQ(6, store_->LastRowID());
  EXPECT_EQ(20, store_->MinTime());

  optional_row_id = store_->FindRowIDFromTimeFirstGreaterThanOrEqual(1);
  ASSERT_TRUE(optional_row_id.has_value());
  EXPECT_EQ(4, optional_row_id.value());
}

TEST_P(HotStoreTest, PushRowBatchesCheckProperties) {
  std::vector<types::Time64NSValue> times = {1, 1, 10, 11};
  std::vector<types::BoolValue> bools = {true, false, true, false};
  std::vector<types::StringValue> strings = {"ab", "cd", "ef", "gh"};
  auto [rb0, _] = MakeRecordOrRowBatch(times, bools, strings);

  EXPECT_EQ(0, store_->Size());

  store_->EmplaceBack(0, std::move(*rb0));
  auto next_row_id = 4;
  EXPECT_EQ(1, store_->Size());

  EXPECT_EQ(0, store_->FirstRowID());
  EXPECT_EQ(3, store_->LastRowID());

  auto optional_row_id = store_->FindRowIDFromTimeFirstGreaterThanOrEqual(1);
  ASSERT_TRUE(optional_row_id.has_value());
  EXPECT_EQ(0, optional_row_id.value());
  optional_row_id = store_->FindRowIDFromTimeFirstGreaterThanOrEqual(12);
  ASSERT_FALSE(optional_row_id.has_value());
  optional_row_id = store_->FindRowIDFromTimeFirstGreaterThan(1);
  ASSERT_TRUE(optional_row_id.has_value());
  EXPECT_EQ(2, optional_row_id.value());
  optional_row_id = store_->FindRowIDFromTimeFirstGreaterThan(11);
  ASSERT_FALSE(optional_row_id.has_value());

  EXPECT_EQ(1, store_->MinTime());

  times = {20, 20, 21};
  bools = {false, false, false};
  strings = {"", "", ""};
  auto [rb1, __] = MakeRecordOrRowBatch(times, bools, strings);

  store_->EmplaceBack(next_row_id, std::move(*rb1));
  EXPECT_EQ(2, store_->Size());

  EXPECT_EQ(0, store_->FirstRowID());
  EXPECT_EQ(6, store_->LastRowID());

  optional_row_id = store_->FindRowIDFromTimeFirstGreaterThanOrEqual(12);
  ASSERT_TRUE(optional_row_id.has_value());
  EXPECT_EQ(4, optional_row_id.value());
  optional_row_id = store_->FindRowIDFromTimeFirstGreaterThanOrEqual(22);
  ASSERT_FALSE(optional_row_id.has_value());
  optional_row_id = store_->FindRowIDFromTimeFirstGreaterThan(20);
  ASSERT_TRUE(optional_row_id.has_value());
  EXPECT_EQ(6, optional_row_id.value());
  optional_row_id = store_->FindRowIDFromTimeFirstGreaterThan(21);
  ASSERT_FALSE(optional_row_id.has_value());

  EXPECT_EQ(1, store_->MinTime());

  store_->PopFront();

  EXPECT_EQ(1, store_->Size());
  EXPECT_EQ(4, store_->FirstRowID());
  EXPECT_EQ(6, store_->LastRowID());
  EXPECT_EQ(20, store_->MinTime());

  optional_row_id = store_->FindRowIDFromTimeFirstGreaterThanOrEqual(1);
  ASSERT_TRUE(optional_row_id.has_value());
  EXPECT_EQ(4, optional_row_id.value());
}

TEST_P(HotStoreTest, RemovePrefix) {
  std::vector<types::Time64NSValue> times = {1, 1, 10, 11};
  std::vector<types::BoolValue> bools = {true, false, true, false};
  std::vector<types::StringValue> strings = {"ab", "cd", "ef", "gh"};
  auto [rb0, _] = MakeRecordOrRowBatch(times, bools, strings);

  store_->EmplaceBack(0, std::move(*rb0));
  auto next_row_id = 4;

  times = {20, 20, 21};
  bools = {false, false, false};
  strings = {"", "", ""};
  auto [rb1, __] = MakeRecordOrRowBatch(times, bools, strings);

  store_->EmplaceBack(next_row_id, std::move(*rb1));

  store_->RemovePrefix(2);
  EXPECT_EQ(2, store_->Size());
  EXPECT_EQ(2, store_->FirstRowID());
  EXPECT_EQ(6, store_->LastRowID());
  EXPECT_EQ(10, store_->MinTime());

  auto optional_row_id = store_->FindRowIDFromTimeFirstGreaterThanOrEqual(1);
  ASSERT_TRUE(optional_row_id.has_value());
  EXPECT_EQ(2, optional_row_id.value());
  optional_row_id = store_->FindRowIDFromTimeFirstGreaterThan(1);
  ASSERT_TRUE(optional_row_id.has_value());
  EXPECT_EQ(2, optional_row_id.value());
}

INSTANTIATE_RECORD_OR_ROW_BATCH_TESTSUITE(HotStore, HotStoreTest, /*include_mixed*/ true);

}  // namespace internal
}  // namespace table_store
}  // namespace px
