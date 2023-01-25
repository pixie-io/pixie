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

#include <arrow/array.h>
#include <arrow/buffer.h>
#include <arrow/builder.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <iostream>
#include <memory>

#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"

namespace px {
namespace types {

TEST(ColumnWrapperTest, make_test_bool) {
  auto wrapper = ColumnWrapper::Make(DataType::BOOLEAN, 10);
  EXPECT_EQ(10, wrapper->Size());
  EXPECT_EQ(DataType::BOOLEAN, wrapper->data_type());
  EXPECT_NE(nullptr, wrapper->UnsafeRawData());

  auto arrow_arr = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_EQ(DataTypeTraits<DataType::BOOLEAN>::arrow_type_id, arrow_arr->type_id());
}

TEST(ColumnWrapperTest, make_test_int64) {
  auto wrapper = ColumnWrapper::Make(DataType::INT64, 10);
  EXPECT_EQ(10, wrapper->Size());
  EXPECT_EQ(DataType::INT64, wrapper->data_type());
  EXPECT_NE(nullptr, wrapper->UnsafeRawData());

  auto arrow_arr = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_EQ(DataTypeTraits<DataType::INT64>::arrow_type_id, arrow_arr->type_id());
}

TEST(ColumnWrapperTest, make_test_uint128) {
  auto wrapper = ColumnWrapper::Make(DataType::UINT128, 10);
  EXPECT_EQ(10, wrapper->Size());
  EXPECT_EQ(DataType::UINT128, wrapper->data_type());
  EXPECT_NE(nullptr, wrapper->UnsafeRawData());

  auto arrow_arr = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_EQ(DataTypeTraits<DataType::UINT128>::arrow_type_id, arrow_arr->type_id());
}

TEST(ColumnWrapperTest, make_test_float64) {
  auto wrapper = ColumnWrapper::Make(DataType::FLOAT64, 10);
  EXPECT_EQ(10, wrapper->Size());
  EXPECT_EQ(DataType::FLOAT64, wrapper->data_type());
  EXPECT_NE(nullptr, wrapper->UnsafeRawData());

  auto arrow_arr = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_EQ(DataTypeTraits<DataType::FLOAT64>::arrow_type_id, arrow_arr->type_id());
}

TEST(ColumnWrapperTest, make_test_string) {
  auto wrapper = ColumnWrapper::Make(DataType::STRING, 10);
  EXPECT_EQ(10, wrapper->Size());
  EXPECT_EQ(DataType::STRING, wrapper->data_type());
  EXPECT_NE(nullptr, wrapper->UnsafeRawData());

  auto arrow_arr = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_EQ(DataTypeTraits<DataType::STRING>::arrow_type_id, arrow_arr->type_id());
}

TEST(ColumnWrapper, FromArrowBool) {
  arrow::BooleanBuilder builder;
  PX_CHECK_OK(builder.Append(true));
  PX_CHECK_OK(builder.Append(true));
  PX_CHECK_OK(builder.Append(false));

  std::shared_ptr<arrow::Array> arr;
  PX_CHECK_OK(builder.Finish(&arr));

  auto wrapper = ColumnWrapper::FromArrow(arr);
  auto converted_to_arrow = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_TRUE(converted_to_arrow->Equals(arr));
}

TEST(ColumnWrapper, FromArrowInt64) {
  arrow::Int64Builder builder;
  PX_CHECK_OK(builder.Append(1));
  PX_CHECK_OK(builder.Append(2));
  PX_CHECK_OK(builder.Append(3));

  std::shared_ptr<arrow::Array> arr;
  PX_CHECK_OK(builder.Finish(&arr));

  auto wrapper = ColumnWrapper::FromArrow(arr);
  auto converted_to_arrow = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_TRUE(converted_to_arrow->Equals(arr));
}

TEST(ColumnWrapper, FromArrowUInt128) {
  arrow::UInt128Builder builder;
  PX_CHECK_OK(builder.Append(absl::MakeUint128(100, 200)));
  PX_CHECK_OK(builder.Append(absl::MakeUint128(200, 300)));
  PX_CHECK_OK(builder.Append(absl::MakeUint128(300, 400)));

  std::shared_ptr<arrow::Array> arr;
  PX_CHECK_OK(builder.Finish(&arr));

  auto wrapper = ColumnWrapper::FromArrow(arr);
  auto converted_to_arrow = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_TRUE(converted_to_arrow->Equals(arr));
}

TEST(ColumnWrapper, FromArrowFloat64) {
  arrow::DoubleBuilder builder;
  PX_CHECK_OK(builder.Append(1));
  PX_CHECK_OK(builder.Append(2));
  PX_CHECK_OK(builder.Append(3));

  std::shared_ptr<arrow::Array> arr;
  PX_CHECK_OK(builder.Finish(&arr));

  auto wrapper = ColumnWrapper::FromArrow(arr);
  auto converted_to_arrow = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_TRUE(converted_to_arrow->ApproxEquals(arr));
}

TEST(ColumnWrapper, FromArrowString) {
  arrow::StringBuilder builder;
  PX_CHECK_OK(builder.Append("abc"));
  PX_CHECK_OK(builder.Append("def"));
  PX_CHECK_OK(builder.Append("hello"));

  std::shared_ptr<arrow::Array> arr;
  PX_CHECK_OK(builder.Finish(&arr));

  auto wrapper = ColumnWrapper::FromArrow(arr);
  auto converted_to_arrow = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_TRUE(converted_to_arrow->Equals(arr));
}

TEST(ColumnWrapperDeathTest, AppendTypeMismatches) {
  auto wrapper = ColumnWrapper::Make(DataType::BOOLEAN, 1);
  ASSERT_EQ(1, wrapper->Size());
  EXPECT_DEATH(wrapper->Append<types::StringValue>("abc"),
               R"(\(1 vs\. 5\) Expect BOOLEAN got STRING)");
}

TEST(ColumnWrapperTest, FromVectorInt64) {
  auto wrapper = ColumnWrapper::Make(DataType::INT64, 4);
  std::vector<types::Int64Value> int_vector({4, 2, 3, 1});
  wrapper->Clear();
  wrapper->AppendFromVector(int_vector);
  auto actual_arr = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_EQ(DataTypeTraits<DataType::INT64>::arrow_type_id, actual_arr->type_id());

  // build the comparison list.
  arrow::Int64Builder builder;
  PX_CHECK_OK(builder.Append(4));
  PX_CHECK_OK(builder.Append(2));
  PX_CHECK_OK(builder.Append(3));
  PX_CHECK_OK(builder.Append(1));

  std::shared_ptr<arrow::Array> expected_arr;
  PX_CHECK_OK(builder.Finish(&expected_arr));

  EXPECT_TRUE(actual_arr->Equals(expected_arr));
}

TEST(ColumnWrapperTest, FromVectorString) {
  auto wrapper = ColumnWrapper::Make(DataType::STRING, 4);
  std::vector<types::StringValue> string_vector({"abc", "def", "ghi", "jkl"});
  wrapper->Clear();
  wrapper->AppendFromVector(string_vector);
  auto actual_arr = wrapper->ConvertToArrow(arrow::default_memory_pool());
  EXPECT_EQ(DataTypeTraits<DataType::STRING>::arrow_type_id, actual_arr->type_id());

  // build the comparison list.
  arrow::StringBuilder builder;
  PX_CHECK_OK(builder.Append("abc"));
  PX_CHECK_OK(builder.Append("def"));
  PX_CHECK_OK(builder.Append("ghi"));
  PX_CHECK_OK(builder.Append("jkl"));

  std::shared_ptr<arrow::Array> expected_arr;
  PX_CHECK_OK(builder.Finish(&expected_arr));

  EXPECT_TRUE(actual_arr->Equals(expected_arr));
}

TEST(ColumnWrapperTest, CopyIndexes) {
  using ::testing::ElementsAreArray;

  auto col = ColumnWrapper::Make(DataType::TIME64NS, 0);
  col->AppendFromVector(std::vector<Time64NSValue>{5, 8, 1, 9, 0, 6, 3, 7, 2, 4});

  // Test reordering all indexes.
  {
    // This reorder sequence should cause the values above to become sorted.
    std::vector<size_t> indexes = {4, 2, 8, 6, 9, 0, 5, 7, 1, 3};

    auto new_col = col->CopyIndexes(indexes);
    for (int i = 0; i < 10; ++i) {
      EXPECT_EQ(new_col->Get<Time64NSValue>(i), i);
    }
  }

  // Test subset selection, with replication.
  {
    std::vector<size_t> indexes = {6, 2, 7, 6};

    auto new_col = col->CopyIndexes(indexes);
    ASSERT_EQ(new_col->Size(), indexes.size());
    EXPECT_EQ(new_col->Get<Time64NSValue>(0), 3);
    EXPECT_EQ(new_col->Get<Time64NSValue>(1), 1);
    EXPECT_EQ(new_col->Get<Time64NSValue>(2), 7);
    EXPECT_EQ(new_col->Get<Time64NSValue>(3), 3);
  }

  // Test empty reorder index.
  {
    std::vector<size_t> indexes = {};

    auto new_col = col->CopyIndexes(indexes);
    ASSERT_EQ(new_col->Size(), 0);
  }
}

// Similar to CopyIndexes test, except we make a new source column each time,
// since MoveIndexes() is destructive on the source.
TEST(ColumnWrapperTest, MoveIndexes) {
  using ::testing::ElementsAreArray;

  // Test reordering all indexes.
  {
    auto col = ColumnWrapper::Make(DataType::TIME64NS, 0);
    col->AppendFromVector(std::vector<Time64NSValue>{5, 8, 1, 9, 0, 6, 3, 7, 2, 4});

    // This reorder sequence should cause the values above to become sorted.
    std::vector<size_t> indexes = {4, 2, 8, 6, 9, 0, 5, 7, 1, 3};

    auto new_col = col->MoveIndexes(indexes);
    for (int i = 0; i < 10; ++i) {
      EXPECT_EQ(new_col->Get<Time64NSValue>(i), i);
    }
  }

  // Test subset selection (unlike CopyIndexes, no replication is allowed).
  {
    auto col = ColumnWrapper::Make(DataType::TIME64NS, 0);
    col->AppendFromVector(std::vector<Time64NSValue>{5, 8, 1, 9, 0, 6, 3, 7, 2, 4});

    std::vector<size_t> indexes = {6, 2, 7};

    auto new_col = col->MoveIndexes(indexes);
    ASSERT_EQ(new_col->Size(), indexes.size());
    EXPECT_EQ(new_col->Get<Time64NSValue>(0), 3);
    EXPECT_EQ(new_col->Get<Time64NSValue>(1), 1);
    EXPECT_EQ(new_col->Get<Time64NSValue>(2), 7);
  }

  // Test empty reorder index.
  {
    auto col = ColumnWrapper::Make(DataType::TIME64NS, 0);
    col->AppendFromVector(std::vector<Time64NSValue>{5, 8, 1, 9, 0, 6, 3, 7, 2, 4});

    std::vector<size_t> indexes = {};

    auto new_col = col->MoveIndexes(indexes);
    ASSERT_EQ(new_col->Size(), 0);
  }
}

}  // namespace types
}  // namespace px
