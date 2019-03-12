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
TEST_F(ExecutorTest, start_executor) {
  VLOG(1) << "start_exectuor";
  auto query =
      "queryDF = From(table='test_table', select=['col1', 'col2']).Result(name='test_output')";

  auto rb_str_status = executor_->ServiceQueryAsString(query);
  ASSERT_OK(rb_str_status);
  auto rb_str = rb_str_status.ConsumeValueOrDie();

  VLOG(1) << absl::StrJoin(rb_str, "\n");
  EXPECT_EQ(absl::StrFormat("\n%s", absl::StrJoin(rb_str, "\n")), kBatchRepr);
}

}  // namespace agent
}  // namespace pl
