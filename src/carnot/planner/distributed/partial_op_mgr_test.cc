#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/partial_op_mgr.h"
#include "src/carnot/udf_exporter/udf_exporter.h"

namespace pl {
namespace carnot {
namespace planner {
namespace distributed {
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

class PartialOpMgrTest : public OperatorTests {};

TEST_F(PartialOpMgrTest, limit_test) {
  auto mem_src = MakeMemSource(MakeRelation());
  auto limit = MakeLimit(mem_src, 10);
  MakeMemSink(limit, "out");

  LimitOperatorMgr mgr;
  EXPECT_TRUE(mgr.Matches(limit));
  auto prepare_limit_or_s = mgr.CreatePrepareOperator(graph.get(), limit);
  ASSERT_OK(prepare_limit_or_s);
  OperatorIR* prepare_limit_uncasted = prepare_limit_or_s.ConsumeValueOrDie();
  ASSERT_MATCH(prepare_limit_uncasted, Limit());
  LimitIR* prepare_limit = static_cast<LimitIR*>(prepare_limit_uncasted);
  EXPECT_EQ(prepare_limit->limit_value(), limit->limit_value());
  EXPECT_EQ(prepare_limit->parents(), limit->parents());
  EXPECT_NE(prepare_limit, limit);

  auto mem_src2 = MakeMemSource(MakeRelation());
  auto merge_limit_or_s = mgr.CreateMergeOperator(graph.get(), mem_src2, limit);
  ASSERT_OK(merge_limit_or_s);
  OperatorIR* merge_limit_uncasted = merge_limit_or_s.ConsumeValueOrDie();
  ASSERT_MATCH(merge_limit_uncasted, Limit());
  LimitIR* merge_limit = static_cast<LimitIR*>(merge_limit_uncasted);
  EXPECT_EQ(merge_limit->limit_value(), limit->limit_value());
  EXPECT_EQ(merge_limit->parents()[0], mem_src2);
  EXPECT_NE(merge_limit, limit);
}
}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
