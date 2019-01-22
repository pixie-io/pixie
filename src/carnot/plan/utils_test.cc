#include <gtest/gtest.h>

#include "src/carnot/plan/utils.h"

namespace pl {
namespace carnot {
namespace plan {

TEST(OpToString, basic_tests) {
  EXPECT_EQ("MemorySourceOperator", ToString(planpb::MEMORY_SOURCE_OPERATOR));
  EXPECT_EQ("MapOperator", ToString(planpb::MAP_OPERATOR));
  EXPECT_EQ("BlockingAggregateOperator", ToString(planpb::BLOCKING_AGGREGATE_OPERATOR));
  EXPECT_EQ("MemorySinkOperator", ToString(planpb::MEMORY_SINK_OPERATOR));
}

TEST(DataTypeToString, basic_tests) {
  EXPECT_EQ("bool", ToString(carnotpb::BOOLEAN));
  EXPECT_EQ("int64", ToString(carnotpb::INT64));
  EXPECT_EQ("float64", ToString(carnotpb::FLOAT64));
  EXPECT_EQ("string", ToString(carnotpb::STRING));
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
