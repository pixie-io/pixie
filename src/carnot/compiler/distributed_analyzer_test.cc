#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <unordered_map>
#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/analyzer.h"
#include "src/carnot/compiler/logical_planner/logical_planner.h"
#include "src/carnot/compiler/logical_planner/test_utils.h"
#include "src/carnot/compiler/parser/parser.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace distributed {

using logical_planner::testutils::CreateTwoAgentsOneKelvinPlannerState;
using logical_planner::testutils::kHttpEventsSchema;
using table_store::schema::Relation;

class DistributedAnalyzerTest : public OperatorTests {
 protected:
  std::unique_ptr<DistributedPlan> PlanQuery(const std::string& query) {
    auto udf_info = udfexporter::ExportUDFInfo().ConsumeValueOrDie()->info_pb();
    auto planner = logical_planner::LogicalPlanner::Create(udf_info).ConsumeValueOrDie();
    // Test via the logical planner for the convenience of being able to test a query string.
    return planner->Plan(CreateTwoAgentsOneKelvinPlannerState(kHttpEventsSchema), query)
        .ConsumeValueOrDie();
  }
};

constexpr char kSimpleQuery[] = R"pxl(
t1 = px.DataFrame(table='http_events', start_time='-120s')
t1.http_resp_latency_ms = t1.http_resp_latency_ns / 1.0E6
px.display(t1)
)pxl";

TEST_F(DistributedAnalyzerTest, resolve_column_indexes) {
  auto plan = PlanQuery(kSimpleQuery);

  std::unordered_map<int64_t, int64_t> expected_num_cols_by_id{{0, 34}, {1, 17}, {2, 17}};
  for (int64_t carnot_id : plan->dag().TopologicalSort()) {
    IR* graph = plan->Get(carnot_id)->plan();
    std::vector<IRNode*> cols = graph->FindNodesOfType(IRNodeType::kColumn);
    EXPECT_EQ(cols.size(), expected_num_cols_by_id.at(carnot_id));
    for (auto col : cols) {
      EXPECT_TRUE(static_cast<ColumnIR*>(col)->is_col_idx_set());
    }
  }
}

}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
