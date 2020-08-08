#include <google/protobuf/text_format.h>

#include <algorithm>
#include <map>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/carnot.h"
#include "src/carnot/exec/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/testing.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {

using exec::CarnotTestUtils;

class JoinTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();
    table_store_ = std::make_shared<table_store::TableStore>();
    carnot_ = Carnot::Create(sole::uuid4(), table_store_, exec::MockResultSinkStubGenerator)
                  .ConsumeValueOrDie();
    auto left_table = CarnotTestUtils::TestTable();
    table_store_->AddTable("left_table", left_table);
    auto right_table = CarnotTestUtils::TestTable();
    table_store_->AddTable("right_table", right_table);
  }

  std::shared_ptr<table_store::TableStore> table_store_;
  std::unique_ptr<Carnot> carnot_;
};

TEST_F(JoinTest, basic) {
  std::string queryString =
      "import px\n"
      "src1 = px.DataFrame(table='left_table', select=['col1', 'col2'])\n"
      "src2 = px.DataFrame(table='right_table', select=['col1', 'col2'])\n"
      "join = src1.merge(src2, how='inner', left_on=['col1', 'col2'], right_on=['col1', 'col2'], "
      "suffixes=['', '_x'])\n"
      "join['left_col1'] = join['col1']\n"
      "join['right_col2'] = join['col2']\n"
      "df = join[['left_col1', 'right_col2']]\n"
      "# fix this\n"
      "px.display(df, 'unused_param')";

  auto query = absl::StrJoin({queryString}, "\n");
  auto query_id = sole::uuid4();
  // No time column, doesn't use a time parameter.
  auto s = carnot_->ExecuteQuery(query, query_id, 0);
  ASSERT_OK(s);
  auto res = s.ConsumeValueOrDie();
  EXPECT_EQ(10, res.rows_processed);
  EXPECT_EQ(10 * sizeof(double) + 10 * sizeof(int64_t), res.bytes_processed);
  EXPECT_GT(res.compile_time_ns, 0);
  EXPECT_GT(res.exec_time_ns, 0);

  // TODO(nserrino/philkuz): Move this logic somewhere more reusable.
  auto table_id = absl::Substitute("$0_$1", query_id.str(), 0);

  auto output_table = table_store_->GetTable(table_id);
  EXPECT_EQ(1, output_table->NumBatches());

  auto rb1 =
      output_table->GetRowBatch(0, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  std::vector<types::Float64Value> expected_col1 = {0.5, 1.2, 5.3, 0.1, 5.1};
  std::vector<types::Int64Value> expected_col2 = {1, 2, 3, 5, 6};
  EXPECT_TRUE(
      rb1->ColumnAt(0)->Equals(types::ToArrow(expected_col1, arrow::default_memory_pool())));
  EXPECT_TRUE(
      rb1->ColumnAt(1)->Equals(types::ToArrow(expected_col2, arrow::default_memory_pool())));
}

TEST_F(JoinTest, self_join) {
  std::string queryString =
      "import px\n"
      "src1 = px.DataFrame(table='left_table', select=['col1', 'col2'])\n"
      "join = src1.merge(src1, how='inner', left_on=['col1'], right_on=['col1'], "
      "suffixes=['', '_x'])\n"
      "join['left_col1'] = join['col1']\n"
      "join['right_col2'] = join['col2_x']\n"
      "output = join[['left_col1', 'right_col2']]\n"
      "px.display(output)";

  auto query = absl::StrJoin({queryString}, "\n");
  auto query_id = sole::uuid4();
  // No time column, doesn't use a time parameter.
  auto s = carnot_->ExecuteQuery(query, query_id, 0);
  ASSERT_OK(s);
  auto res = s.ConsumeValueOrDie();
  EXPECT_EQ(5, res.rows_processed);
  EXPECT_EQ(5 * sizeof(double) + 5 * sizeof(int64_t), res.bytes_processed);
  EXPECT_GT(res.compile_time_ns, 0);
  EXPECT_GT(res.exec_time_ns, 0);

  // TODO(nserrino/philkuz): Move this logic somewhere more reusable.
  auto table_id = absl::Substitute("$0_$1", query_id.str(), 0);

  auto output_table = table_store_->GetTable(table_id);
  EXPECT_EQ(1, output_table->NumBatches());

  auto rb1 =
      output_table->GetRowBatch(0, std::vector<int64_t>({0, 1}), arrow::default_memory_pool())
          .ConsumeValueOrDie();
  std::vector<types::Float64Value> expected_col1 = {0.5, 1.2, 5.3, 0.1, 5.1};
  std::vector<types::Int64Value> expected_col2 = {1, 2, 3, 5, 6};
  EXPECT_TRUE(
      rb1->ColumnAt(0)->Equals(types::ToArrow(expected_col1, arrow::default_memory_pool())));
  EXPECT_TRUE(
      rb1->ColumnAt(1)->Equals(types::ToArrow(expected_col2, arrow::default_memory_pool())));
}

}  // namespace carnot
}  // namespace pl
