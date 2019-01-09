#include <gtest/gtest.h>

#include "src/carnot/plan/operators.h"
#include "src/carnot/plan/proto/plan.pb.h"

namespace pl {
namespace carnot {
namespace plan {

TEST(OpToString, basic_tests) {
  EXPECT_EQ("MemorySourceOperator", ToString(planpb::MEMORY_SOURCE_OPERATOR));
  EXPECT_EQ("MapOperator", ToString(planpb::MAP_OPERATOR));
  EXPECT_EQ("BlockingAggregateOperator", ToString(planpb::BLOCKING_AGGREGATE_OPERATOR));
  EXPECT_EQ("MemorySinkOperator", ToString(planpb::MEMORY_SINK_OPERATOR));
}

// TODO(zasgar): Add tests for the node after adding test fixtures and class implementations.

}  // namespace plan
}  // namespace carnot
}  // namespace pl
