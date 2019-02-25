#include <gtest/gtest.h>

#include "src/carnot/plan/utils.h"
#include "src/common/type_utils.h"

namespace pl {
namespace carnot {
namespace plan {

TEST(OpToString, basic_tests) {
  EXPECT_EQ("MemorySourceOperator", ToString(carnotpb::MEMORY_SOURCE_OPERATOR));
  EXPECT_EQ("MapOperator", ToString(carnotpb::MAP_OPERATOR));
  EXPECT_EQ("BlockingAggregateOperator", ToString(carnotpb::BLOCKING_AGGREGATE_OPERATOR));
  EXPECT_EQ("MemorySinkOperator", ToString(carnotpb::MEMORY_SINK_OPERATOR));
}

TEST(DataTypeToString, basic_tests) {
  EXPECT_EQ("bool", types::ToString(types::BOOLEAN));
  EXPECT_EQ("int64", types::ToString(types::INT64));
  EXPECT_EQ("float64", types::ToString(types::FLOAT64));
  EXPECT_EQ("string", types::ToString(types::STRING));
  EXPECT_EQ("time64ns", types::ToString(types::TIME64NS));
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
