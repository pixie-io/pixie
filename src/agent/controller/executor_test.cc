#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/agent/controller/executor.h"
#include "src/carnot/exec/test_utils.h"

namespace pl {
namespace agent {

using carnot::exec::CarnotTestUtils;
using types::Float64Value;
using types::Int64Value;
using types::ToArrow;

class ExecutorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto registry_ = std::make_unique<stirling::SourceRegistry>();
    auto carnot_ = std::make_unique<Carnot>();
    PL_CHECK_OK(carnot_->Init());
    // only testing execution of query. (PL-398) bpftrace causes memory leak and fails ASAN.
    executor_ = std::make_unique<Executor>(std::move(carnot_), nullptr);

    PL_CHECK_OK(executor_->AddDummyTable("test_table", CarnotTestUtils::TestTable()));
  }
  std::unique_ptr<Executor> executor_;
};
const char* kBatchRepr = R"(
col1:   [
    0.5,
    1.2,
    5.3
  ]
col2:   [
    1,
    2,
    3
  ]

col1:   [
    0.1,
    5.1
  ]
col2:   [
    5,
    6
  ]
)";

TEST_F(ExecutorTest, should_execute_simple_query_and_return_results) {
  VLOG(1) << "start_exectuor";
  auto query =
      "queryDF = From(table='test_table', select=['col1', 'col2']).Result(name='test_output')";
  vizier::AgentQueryResponse query_response;
  auto rb_str_status = executor_->ServiceQuery(query, &query_response, 0);
  ASSERT_OK(rb_str_status);
  EXPECT_OK(Status(query_response.status()));
  // Should be one table.
  EXPECT_EQ(1, query_response.tables().size());
  auto& table0 = query_response.tables(0);
  EXPECT_EQ(2, table0.data().size());

  // Check the output data.
  auto& rb1 = table0.data(0);
  EXPECT_EQ(2, rb1.cols_size());
  EXPECT_THAT(rb1.cols(0).float64_data().data(), testing::ElementsAre(0.5, 1.2, 5.3));
  EXPECT_THAT(rb1.cols(1).int64_data().data(), testing::ElementsAre(1, 2, 3));

  auto& rb2 = table0.data(1);
  EXPECT_THAT(rb2.cols(0).float64_data().data(), testing::ElementsAre(0.1, 5.1));
  EXPECT_THAT(rb2.cols(1).int64_data().data(), testing::ElementsAre(5, 6));

  // Check relation.
  auto& relation = table0.relation();
  ASSERT_EQ(2, relation.columns_size());
  auto& rel_col0 = relation.columns(0);
  auto& rel_col1 = relation.columns(1);
  EXPECT_EQ("col1", rel_col0.column_name());
  EXPECT_EQ("col2", rel_col1.column_name());

  EXPECT_EQ(types::FLOAT64, rel_col0.column_type());
  EXPECT_EQ(types::INT64, rel_col1.column_type());
}

}  // namespace agent
}  // namespace pl
