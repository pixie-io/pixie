#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/distributed/distributed_planner.h"
#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/logical_planner/logical_planner.h"
#include "src/carnot/compiler/logical_planner/test_utils.h"
#include "src/carnot/compiler/rules/rules.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/protobuf.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace logical_planner {
using pl::testing::proto::EqualsProto;

class LogicalPlannerTest : public ::testing::Test {
 protected:
  void SetUp() { info_ = udfexporter::ExportUDFInfo().ConsumeValueOrDie()->info_pb(); }
  plannerpb::QueryRequest MakeQueryRequest(const std::string& query) {
    plannerpb::QueryRequest query_request;
    query_request.set_query_str(query);
    return query_request;
  }
  udfspb::UDFInfo info_;
};

TEST_F(LogicalPlannerTest, two_agents_one_kelvin) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  auto plan = planner
                  ->Plan(testutils::CreateTwoAgentsOneKelvinPlannerState(),
                         MakeQueryRequest(testutils::kQueryForTwoAgents))
                  .ConsumeValueOrDie();
  auto out_pb = plan->ToProto().ConsumeValueOrDie();
  EXPECT_THAT(out_pb, Partially(EqualsProto(testutils::kExpectedPlanTwoAgentOneKelvin)))
      << out_pb.DebugString();
}

TEST_F(LogicalPlannerTest, distributed_plan_test_basic_queries) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  auto ps = testutils::CreateTwoAgentsOneKelvinPlannerState(testutils::kHttpEventsSchema);
  auto plan_or_s = planner->Plan(ps, MakeQueryRequest(testutils::kHttpRequestStats));
  EXPECT_OK(plan_or_s);
  auto plan = plan_or_s.ConsumeValueOrDie();
  EXPECT_OK(plan->ToProto());
}

constexpr char kSimpleQueryDefaultLimit[] = R"pxl(
t1 = px.DataFrame(table='http_events', start_time='-120s', select=['time_'])
px.display(t1)
)pxl";

TEST_F(LogicalPlannerTest, max_output_rows) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  auto state = testutils::CreateTwoAgentsOneKelvinPlannerState(testutils::kHttpEventsSchema);
  state.mutable_plan_options()->set_max_output_rows_per_table(100);
  auto plan_or_s = planner->Plan(state, MakeQueryRequest(kSimpleQueryDefaultLimit));
  EXPECT_OK(plan_or_s);
  auto plan = plan_or_s.ConsumeValueOrDie();

  for (const auto& id : plan->dag().TopologicalSort()) {
    auto subgraph = plan->Get(id)->plan();
    auto limits = subgraph->FindNodesOfType(IRNodeType::kLimit);
    EXPECT_EQ(1, limits.size());
    EXPECT_EQ(100, static_cast<LimitIR*>(limits[0])->limit_value());
  }

  EXPECT_OK(plan->ToProto());
}

constexpr char kCompileTimeQuery[] = R"pxl(
t1 = px.DataFrame(table='http_events', start_time='-120s')

t1['service'] = t1.ctx['service']
t1['http_resp_latency_ms'] = t1['http_resp_latency_ns'] / 1.0E6
t1['failure'] = t1['http_resp_status'] >= 400
t1['range_group'] = t1['time_'] - px.modulo(t1['time_'], 2000000000)
t1['s'] = px.bin(t1['time_'],px.seconds(3))

quantiles_agg = t1.groupby('service').agg(
  latency_quantiles=('http_resp_latency_ms', px.quantiles),
  errors=('failure', px.mean),
  throughput_total=('http_resp_status', px.count),
)

quantiles_agg['latency_p50'] = px.pluck(quantiles_agg['latency_quantiles'], 'p50')
quantiles_agg['latency_p90'] = px.pluck(quantiles_agg['latency_quantiles'], 'p90')
quantiles_agg['latency_p99'] = px.pluck(quantiles_agg['latency_quantiles'], 'p99')
quantiles_table = quantiles_agg[['service', 'latency_p50', 'latency_p90', 'latency_p99', 'errors', 'throughput_total']]

# The Range aggregate to calcualte the requests per second.
requests_agg = t1.groupby(['service', 'range_group']).agg(
  requests_per_window=('http_resp_status', px.count),
)

rps_table = requests_agg.groupby('service').agg(rps=('requests_per_window',px.mean))

joined_table = quantiles_table.merge(rps_table,
                                     how='inner',
                                     left_on=['service'],
                                     right_on=['service'],
                                     suffixes=['', '_x'])

joined_table['latency(p50)'] = joined_table['latency_p50']
joined_table['latency(p90)'] = joined_table['latency_p90']
joined_table['latency(p99)'] = joined_table['latency_p99']
joined_table['throughput (rps)'] = joined_table['rps']
joined_table['throughput total'] = joined_table['throughput_total']

joined_table = joined_table[[
  'service',
  'latency(p50)',
  'latency(p90)',
  'latency(p99)',
  'errors',
  'throughput (rps)',
  'throughput total']]
joined_table['asid'] = px.asid()
joined_table = joined_table[joined_table['service'] != '' and px.asid() == 3870]
px.display(joined_table)
)pxl";

TEST_F(LogicalPlannerTest, duplicate_int) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  auto plan_or_s =
      planner->Plan(testutils::CreateTwoAgentsOneKelvinPlannerState(testutils::kHttpEventsSchema),
                    MakeQueryRequest(kCompileTimeQuery));
  EXPECT_OK(plan_or_s);
  auto plan = plan_or_s.ConsumeValueOrDie();
  EXPECT_OK(plan->ToProto());
}

constexpr char kTwoWindowQuery[] = R"query(
t1 = px.DataFrame(table='http_events', start_time='-300s')
t1['service'] = t1.ctx['service']
t1['http_resp_latency_ms'] = t1['http_resp_latency_ns'] / 1.0E6
# edit this to increase/decrease window. Dont go lower than 1 second.
t1['window1'] = px.bin(t1['time_'], px.seconds(10))
t1['window2'] = px.bin(t1['time_'] + px.seconds(5), px.seconds(10))
# groupby 1sec intervals per window
window1_agg = t1.groupby(['service', 'window1']).agg(
  quantiles=('http_resp_latency_ms', px.quantiles),
)
window1_agg['p50'] = px.pluck(window1_agg['quantiles'], 'p50')
window1_agg['p90'] = px.pluck(window1_agg['quantiles'], 'p90')
window1_agg['p99'] = px.pluck(window1_agg['quantiles'], 'p99')
window1_agg['time_'] = window1_agg['window1']
# window1_agg = window1_agg.drop('window1')

window2_agg = t1.groupby(['service', 'window2']).agg(
  quantiles=('http_resp_latency_ms', px.quantiles),
)
window2_agg['p50'] = px.pluck(window2_agg['quantiles'], 'p50')
window2_agg['p90'] = px.pluck(window2_agg['quantiles'], 'p90')
window2_agg['p99'] = px.pluck(window2_agg['quantiles'], 'p99')
window2_agg['time_'] = window2_agg['window2']
# window2_agg = window2_agg.drop('window2')

df = window2_agg[window2_agg['service'] != '']
px.display(df)
)query";
TEST_F(LogicalPlannerTest, NestedCompileTime) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  auto plan_or_s =
      planner->Plan(testutils::CreateTwoAgentsOneKelvinPlannerState(testutils::kHttpEventsSchema),
                    MakeQueryRequest(kTwoWindowQuery));
  EXPECT_OK(plan_or_s);
  auto plan = plan_or_s.ConsumeValueOrDie();
  EXPECT_OK(plan->ToProto());
}

constexpr char kAppendQuery[] = R"pxl(
df1 = px.DataFrame(table='http_events', start_time='-2m', select=['time_', 'upid'])
df2 = px.DataFrame(table='http_events', start_time='-3m', select=['time_', 'upid'])
px.display(df1.append(df2))
)pxl";

TEST_F(LogicalPlannerTest, AppendTest) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  auto plan_or_s =
      planner->Plan(testutils::CreateTwoAgentsOneKelvinPlannerState(testutils::kHttpEventsSchema),
                    MakeQueryRequest(kAppendQuery));
  EXPECT_OK(plan_or_s);
  auto plan = plan_or_s.ConsumeValueOrDie();
  EXPECT_OK(plan->ToProto());
}

constexpr char kAppendSelfQuery[] = R"pxl(
df1 = px.DataFrame(table='http_events', start_time='-5m', select=['time_', 'upid'])
df2 = df1[2==2]
px.display(df1.append(df2))
)pxl";

TEST_F(LogicalPlannerTest, AppendSelfTest) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  auto plan_or_s =
      planner->Plan(testutils::CreateTwoAgentsOneKelvinPlannerState(testutils::kHttpEventsSchema),
                    MakeQueryRequest(kAppendSelfQuery));
  EXPECT_OK(plan_or_s);
  auto plan = plan_or_s.ConsumeValueOrDie();
  EXPECT_OK(plan->ToProto());
}
}  // namespace logical_planner
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
