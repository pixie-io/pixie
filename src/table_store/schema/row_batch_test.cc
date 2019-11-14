#include <arrow/array.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <vector>

#include "src/common/testing/testing.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/proto/types.pb.h"
#include "src/shared/types/types.h"
#include "src/table_store/proto/schema.pb.h"
#include "src/table_store/schema/row_batch.h"
#include "src/table_store/schema/row_descriptor.h"

namespace pl {
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

    RowDescriptor rd = RowDescriptor(descriptor);
    rb_ = std::make_unique<RowBatch>(rd, 3);
  }
  std::unique_ptr<RowBatch> rb_;
};

TEST_F(RowBatchTest, basic_test) {
  std::vector<types::BoolValue> in1 = {true, false, true};
  std::vector<types::Int64Value> in2 = {3, 4, 5};
  std::vector<types::Float64Value> in3 = {3.3, 4.1, 5.6};

  EXPECT_TRUE(rb_->AddColumn(types::ToArrow(in1, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(rb_->AddColumn(types::ToArrow(in2, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(rb_->AddColumn(types::ToArrow(in3, arrow::default_memory_pool())).ok());

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
  std::vector<types::BoolValue> in1 = {true, false, true};
  std::vector<types::Int64Value> in2 = {3, 4, 5};
  std::vector<types::Float64Value> in3 = {3.3, 4.1, 5.6};
  std::vector<types::Float64Value> in4 = {3.3, 4.1, 5.6, 1.2};

  EXPECT_TRUE(rb_->AddColumn(types::ToArrow(in1, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(rb_->AddColumn(types::ToArrow(in2, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(rb_->AddColumn(types::ToArrow(in3, arrow::default_memory_pool())).ok());
  EXPECT_FALSE(rb_->AddColumn(types::ToArrow(in4, arrow::default_memory_pool())).ok());
}

TEST_F(RowBatchTest, extra_row_test) {
  std::vector<types::BoolValue> in1 = {false, true, true, true};
  EXPECT_FALSE(rb_->AddColumn(types::ToArrow(in1, arrow::default_memory_pool())).ok());
}

TEST_F(RowBatchTest, incorrect_type_test) {
  std::vector<types::Float64Value> in1 = {1.4, 1.2, 1.1};
  EXPECT_FALSE(rb_->AddColumn(types::ToArrow(in1, arrow::default_memory_pool())).ok());
}

TEST_F(RowBatchTest, num_bytes) {
  auto descriptor =
      std::vector<types::DataType>({types::DataType::BOOLEAN, types::DataType::INT64,
                                    types::DataType::FLOAT64, types::DataType::STRING});

  RowDescriptor rd = RowDescriptor(descriptor);
  auto rb_ = std::make_unique<RowBatch>(rd, 3);

  std::vector<types::BoolValue> in1 = {true, false, true};
  std::vector<types::Int64Value> in2 = {3, 4, 5};
  std::vector<types::Float64Value> in3 = {3.3, 4.1, 5.6};
  std::vector<types::StringValue> in4 = {"hello", "thisIs", "aString"};

  EXPECT_TRUE(rb_->AddColumn(types::ToArrow(in1, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(rb_->AddColumn(types::ToArrow(in2, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(rb_->AddColumn(types::ToArrow(in3, arrow::default_memory_pool())).ok());
  EXPECT_TRUE(rb_->AddColumn(types::ToArrow(in4, arrow::default_memory_pool())).ok());

  auto expected_bytes =
      3 * sizeof(bool) + 3 * sizeof(int64_t) + 3 * sizeof(double) + sizeof(char) * 18;
  EXPECT_EQ(expected_bytes, rb_->NumBytes());
}

TEST_F(RowBatchTest, to_from_proto) {
  std::string input_proto_string = R"(
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

  table_store::schemapb::RowBatchData input_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(input_proto_string, &input_proto));

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

}  // namespace schema
}  // namespace table_store
}  // namespace pl
