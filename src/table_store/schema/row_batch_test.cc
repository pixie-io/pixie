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
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <vector>

#include "src/common/testing/testing.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/types.h"
#include "src/shared/types/typespb/wrapper/types_pb_wrapper.h"
#include "src/table_store/schema/row_batch.h"
#include "src/table_store/schema/row_descriptor.h"
#include "src/table_store/schemapb/schema.pb.h"

namespace px {
namespace table_store {
namespace schema {

TEST(RowDescriptorTest, basic_test) {
  auto descriptor = std::vector<types::DataType>(
      {types::DataType::BOOLEAN, types::DataType::INT64, types::DataType::FLOAT64});
  RowDescriptor rd = RowDescriptor(descriptor);
  EXPECT_EQ(rd.type(0), types::DataType::BOOLEAN);
  EXPECT_EQ(rd.type(1), types::DataType::INT64);
  EXPECT_EQ(rd.type(2), types::DataType::FLOAT64);
}

class RowBatchTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto descriptor = std::vector<types::DataType>(
        {types::DataType::BOOLEAN, types::DataType::INT64, types::DataType::FLOAT64});

    rd_ = std::make_unique<RowDescriptor>(descriptor);
    rb_ = std::make_unique<RowBatch>(*rd_, 3);
    AddColumns(rb_.get());
  }
  void AddColumns(RowBatch* rb) {
    std::vector<types::BoolValue> in1 = {true, false, true};
    std::vector<types::Int64Value> in2 = {3, 4, 5};
    std::vector<types::Float64Value> in3 = {3.3, 4.1, 5.6};

    EXPECT_TRUE(rb->AddColumn(types::ToArrow(in1, arrow::default_memory_pool())).ok());
    EXPECT_TRUE(rb->AddColumn(types::ToArrow(in2, arrow::default_memory_pool())).ok());
    EXPECT_TRUE(rb->AddColumn(types::ToArrow(in3, arrow::default_memory_pool())).ok());
  }
  std::unique_ptr<RowBatch> rb_;
  std::unique_ptr<RowDescriptor> rd_;
};

TEST_F(RowBatchTest, basic_test) {
  EXPECT_TRUE(rb_->HasColumn(0));
  EXPECT_TRUE(rb_->HasColumn(1));
  EXPECT_TRUE(rb_->HasColumn(2));
  EXPECT_FALSE(rb_->HasColumn(3));

  EXPECT_EQ(3, rb_->num_rows());
  EXPECT_EQ(3, rb_->num_columns());
  EXPECT_EQ(
      "RowBatch(eow=0, eos=0):\n  [\n  true,\n  false,\n  true\n]\n  [\n  3,\n  4,\n  5\n]\n  [\n  "
      "3.3,\n  "
      "4.1,\n  5.6\n]\n",
      rb_->DebugString());
}

TEST_F(RowBatchTest, extra_col_test) {
  std::vector<types::BoolValue> in4 = {true, false, true};
  EXPECT_FALSE(rb_->AddColumn(types::ToArrow(in4, arrow::default_memory_pool())).ok());
}

TEST_F(RowBatchTest, extra_row_test) {
  std::vector<types::BoolValue> in1 = {false, true, true, true};
  auto rb = std::make_unique<RowBatch>(*rd_, 3);
  EXPECT_FALSE(rb->AddColumn(types::ToArrow(in1, arrow::default_memory_pool())).ok());
  // This is done at the end so that the check in the RowBatch destructor succeeds.
  AddColumns(rb.get());
}

TEST_F(RowBatchTest, incorrect_type_test) {
  std::vector<types::Float64Value> in1 = {1.4, 1.2, 1.1};
  auto rb = std::make_unique<RowBatch>(*rd_, 3);
  EXPECT_FALSE(rb->AddColumn(types::ToArrow(in1, arrow::default_memory_pool())).ok());
  // This is done at the end so that the check in the RowBatch destructor succeeds.
  AddColumns(rb.get());
}

TEST_F(RowBatchTest, num_bytes) {
  auto descriptor =
      std::vector<types::DataType>({types::DataType::BOOLEAN, types::DataType::INT64,
                                    types::DataType::FLOAT64, types::DataType::STRING});

  RowDescriptor rd = RowDescriptor(descriptor);
  auto rb = std::make_unique<RowBatch>(rd, 3);

  std::vector<types::BoolValue> in1 = {true, false, true};
  std::vector<types::Int64Value> in2 = {3, 4, 5};
  std::vector<types::Float64Value> in3 = {3.3, 4.1, 5.6};
  std::vector<types::StringValue> in4 = {"hello", "thisIs", "aString"};

  EXPECT_TRUE(rb->AddColumn(types::ToArrow(in1, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(rb->AddColumn(types::ToArrow(in2, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(rb->AddColumn(types::ToArrow(in3, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(rb->AddColumn(types::ToArrow(in4, arrow::default_memory_pool())).ok());

  auto expected_bytes =
      3 * sizeof(bool) + 3 * sizeof(int64_t) + 3 * sizeof(double) + sizeof(char) * 18;
  EXPECT_EQ(expected_bytes, rb->NumBytes());
}

constexpr char kTestRowBatchProto[] = R"(
cols {
  uint128_data {
    data {
      low: 1
      high: 2
    }
    data {
      low: 3
      high: 4
    }
    data {
      low: 5
      high: 6
    }
  }
}
cols {
  int64_data {
    data: 1
    data: 2
    data: 3
  }
}
cols {
  string_data {
    data: "ABC"
    data: "DEF"
    data: "12345"
  }
}
eow: true
eos: false
num_rows: 3
)";
TEST_F(RowBatchTest, to_from_proto) {
  table_store::schemapb::RowBatchData input_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kTestRowBatchProto, &input_proto));

  auto rb = RowBatch::FromProto(input_proto).ConsumeValueOrDie();
  EXPECT_TRUE(rb->eow());
  EXPECT_FALSE(rb->eos());
  EXPECT_EQ(3, rb->num_rows());
  // Check the types of the output columns.
  EXPECT_EQ(types::DataType::UINT128, rb->desc().type(0));
  EXPECT_EQ(types::DataType::INT64, rb->desc().type(1));
  EXPECT_EQ(types::DataType::STRING, rb->desc().type(2));

  table_store::schemapb::RowBatchData output_proto;
  EXPECT_OK(rb->ToProto(&output_proto));

  google::protobuf::util::MessageDifferencer differ;
  EXPECT_TRUE(differ.Compare(input_proto, output_proto));
}

TEST_F(RowBatchTest, with_zero_rows) {
  bool eow = true;
  bool eos = false;
  auto rb = RowBatch::WithZeroRows(*rd_, eow, eos).ConsumeValueOrDie();
  EXPECT_EQ(3, rb->num_columns());
  EXPECT_EQ(0, rb->num_rows());
  EXPECT_EQ(*rd_, rb->desc());
  EXPECT_EQ(eow, rb->eow());
  EXPECT_EQ(eos, rb->eos());
}

TEST_F(RowBatchTest, slice) {
  EXPECT_EQ(3, rb_->num_rows());

  ASSERT_OK_AND_ASSIGN(auto output_rb1, rb_->Slice(1, 2));
  EXPECT_EQ(2, output_rb1->num_rows());
  EXPECT_EQ(
      "RowBatch(eow=0, eos=0):\n  [\n  false,\n  true\n]\n  [\n  4,\n  5\n]\n  [\n  "
      "4.1,\n  5.6\n]\n",
      output_rb1->DebugString());

  ASSERT_OK_AND_ASSIGN(auto output_rb2, rb_->Slice(1, 1));
  EXPECT_EQ(1, output_rb2->num_rows());
  EXPECT_EQ("RowBatch(eow=0, eos=0):\n  [\n  false\n]\n  [\n  4\n]\n  [\n  4.1\n]\n",
            output_rb2->DebugString());

  ASSERT_OK_AND_ASSIGN(auto output_rb3, rb_->Slice(0, 1));
  EXPECT_EQ(1, output_rb3->num_rows());
  EXPECT_EQ("RowBatch(eow=0, eos=0):\n  [\n  true\n]\n  [\n  3\n]\n  [\n  3.3\n]\n",
            output_rb3->DebugString());
  // EOS and EOW don't propagate.
  rb_->set_eos(true);
  rb_->set_eow(true);

  EXPECT_TRUE(rb_->eos());
  EXPECT_TRUE(rb_->eow());

  ASSERT_OK_AND_ASSIGN(auto output_rb4, rb_->Slice(0, 1));
  EXPECT_FALSE(output_rb4->eow());
  EXPECT_FALSE(output_rb4->eos());
  EXPECT_EQ(output_rb4->DebugString(), output_rb3->DebugString());

  // Error out if the slice doesn't fit the bounds.
  auto status1 = rb_->Slice(1, 3);
  ASSERT_NOT_OK(status1);
  ASSERT_EQ(status1.msg(), "Slice(offset=1, length=3) on rowbatch of length 3 is invalid");

  auto status2 = rb_->Slice(-1, 3);
  ASSERT_NOT_OK(status2);
  ASSERT_EQ(status2.msg(), "Slice(offset=-1, length=3) on rowbatch of length 3 is invalid");
}

}  // namespace schema
}  // namespace table_store
}  // namespace px
