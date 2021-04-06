#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/carnot/planner/distributed/presplit_optimizer.h"
#include "src/carnot/planner/test_utils.h"

namespace pl {
namespace carnot {
namespace planner {
namespace distributed {

using table_store::schema::Relation;
using table_store::schemapb::Schema;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Return;
using testutils::DistributedRulesTest;

TEST_F(DistributedRulesTest, PreSplitOptimizerLimitPushdown) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});

  MemorySourceIR* src = MakeMemSource(relation);
  MapIR* map1 = MakeMap(src, {{"abc", MakeColumn("abc", 0)}}, false);
  MapIR* map2 = MakeMap(map1, {{"def", MakeColumn("abc", 0)}}, false);
  LimitIR* limit = MakeLimit(map2, 10);
  MemorySinkIR* sink = MakeMemSink(limit, "foo", {});

  auto optimizer = PreSplitOptimizer::Create(compiler_state_.get()).ConsumeValueOrDie();
  ASSERT_OK(optimizer->Execute(graph.get()));

  EXPECT_THAT(sink->parents(), ElementsAre(map2));
  EXPECT_THAT(map2->parents(), ElementsAre(map1));
  EXPECT_EQ(1, map1->parents().size());
  auto new_limit = map1->parents()[0];
  EXPECT_MATCH(new_limit, Limit());
  EXPECT_EQ(10, static_cast<LimitIR*>(new_limit)->limit_value());
  EXPECT_THAT(new_limit->parents(), ElementsAre(src));

  // Ensure the limit inherits the relation from its parent, not the previous location.
  EXPECT_EQ(new_limit->relation(), relation);
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
