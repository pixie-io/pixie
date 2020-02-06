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
    plannerpb::QueryRequest query_request;
    query_request.set_query_str(query);
    // Test via the logical planner for the convenience of being able to test a query string.
    return planner->Plan(CreateTwoAgentsOneKelvinPlannerState(kHttpEventsSchema), query_request)
        .ConsumeValueOrDie();
  }
};

}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
