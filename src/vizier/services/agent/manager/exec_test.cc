#include <gtest/gtest.h>

#include "src/vizier/services/agent/manager/exec.h"

#include "src/common/testing/testing.h"

namespace pl {
namespace vizier {
namespace agent {

constexpr char kPlanFragmentTmpl[] = R"proto(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 1
      sorted_children: 2
    }
    nodes {
      id: 2
    }
  }
  nodes {
    id: 1
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "cpu"
        column_idxs: 0
        column_names: "count"
        column_types: INT64
      }
    }
  }
  nodes {
    id: 2
    op $0
  }
}
)proto";

constexpr char kMemSinkOp[] = R"(
{
  op_type: MEMORY_SINK_OPERATOR
  mem_sink_op {
    name: "out"
    column_names: "count"
    column_types: INT64
    column_semantic_types: ST_NONE
  }
}
)";

constexpr char kGRPCSinkOp[] = R"(
{
  op_type: GRPC_SINK_OPERATOR
  grpc_sink_op {
    address: "localhost:1234"
    grpc_source_id: 0
  }
}
)";

TEST(PlanContainsBatchResultsTest, WithBatchResults) {
  carnot::planpb::Plan plan_pb;
  auto plan_str = absl::Substitute(kPlanFragmentTmpl, kMemSinkOp);
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(plan_str, &plan_pb));

  auto res = PlanContainsBatchResults(plan_pb);
  ASSERT_OK(res);
  ASSERT_TRUE(res.ConsumeValueOrDie());
}

TEST(PlanContainsBatchResultsTest, WithoutBatchResults) {
  carnot::planpb::Plan plan_pb;
  auto plan_str = absl::Substitute(kPlanFragmentTmpl, kGRPCSinkOp);
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(plan_str, &plan_pb));

  auto res = PlanContainsBatchResults(plan_pb);
  ASSERT_OK(res);
  ASSERT_FALSE(res.ConsumeValueOrDie());
}

}  // namespace agent
}  // namespace vizier
}  // namespace pl
