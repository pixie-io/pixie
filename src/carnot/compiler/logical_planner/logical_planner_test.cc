#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/distributed_planner.h"
#include "src/carnot/compiler/distributedpb/test_proto.h"
#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/metadata_handler.h"
#include "src/carnot/compiler/rule_mock.h"
#include "src/carnot/compiler/rules.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/protobuf.h"

#include "src/carnot/compiler/logical_planner/logical_planner.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace logical_planner {
using pl::testing::proto::EqualsProto;

const char* kTwoAgentOneKelvinDistributedState = R"proto(
carnot_info {
  query_broker_address: "agent1"
  has_grpc_server: false
  has_data_store: true
  processes_data: true
  accepts_remote_sources: false
  table_info {
    table: "table1"
    tabletization_key: "upid"
    tablets: "1"
    tablets: "2"
  }
}
carnot_info {
  query_broker_address: "agent2"
  has_grpc_server: false
  has_data_store: true
  processes_data: true
  accepts_remote_sources: false
  table_info {
    table: "table1"
    tabletization_key: "upid"
    tablets: "3"
    tablets: "4"
  }
}
carnot_info {
  query_broker_address: "kelvin"
  grpc_address: "1111"
  has_grpc_server: true
  has_data_store: false
  processes_data: true
  accepts_remote_sources: true
}
)proto";

class LogicalPlannerTest : public ::testing::Test {
 protected:
};

TEST_F(LogicalPlannerTest, two_agents_one_kelvin) {
  auto planner = LogicalPlanner::Create(true).ConsumeValueOrDie();
  auto plan = planner
                  ->Plan(distributedpb::testutils::CreateTwoAgentsOneKelvinPlannerState(),
                         distributedpb::testutils::kQueryForTwoAgents)
                  .ConsumeValueOrDie();
  auto out_pb = plan->ToProto().ConsumeValueOrDie();
  EXPECT_THAT(out_pb,
              Partially(EqualsProto(distributedpb::testutils::kExpectedPlanTwoAgentOneKelvin)))
      << out_pb.DebugString();
}

TEST_F(LogicalPlannerTest, many_agents) {
  auto planner = LogicalPlanner::Create(false).ConsumeValueOrDie();
  auto plan = planner
                  ->Plan(distributedpb::testutils::CreateTwoAgentsPlannerState(),
                         distributedpb::testutils::kQueryForTwoAgents)
                  .ConsumeValueOrDie();
  auto out_pb = plan->ToProto().ConsumeValueOrDie();
  EXPECT_THAT(out_pb, Partially(EqualsProto(distributedpb::testutils::kExpectedPlanTwoAgents)));
}

const char* kHttpRequestStats = R"pxl(
t1 = pl.DataFrame(table='http_events', start_time='-30s')

t1['service'] = t1.attr['service']
t1['http_resp_latency_ms'] = t1['http_resp_latency_ns'] / 1.0E6
t1['failure'] = t1['http_resp_status'] >= 400
t1['range_group'] = t1['time_'] - pl.modulo(t1['time_'], 1000000000)

quantiles_agg = t1.groupby('service').agg(
  latency_quantiles=('http_resp_latency_ms', pl.quantiles),
  errors=('failure', pl.mean),
  throughput_total=('http_resp_status', pl.count),
)

quantiles_agg['latency_p50'] = pl.pluck(quantiles_agg['latency_quantiles'], 'p50')
quantiles_agg['latency_p90'] = pl.pluck(quantiles_agg['latency_quantiles'], 'p90')
quantiles_agg['latency_p99'] = pl.pluck(quantiles_agg['latency_quantiles'], 'p99')
quantiles_table = quantiles_agg[['service', 'latency_p50', 'latency_p90', 'latency_p99', 'errors', 'throughput_total']]

# The Range aggregate to calcualte the requests per second.
requests_agg = t1.groupby(['service', 'range_group']).agg(
  requests_per_window=('http_resp_status', pl.count),
)

rps_table = requests_agg.groupby('service').agg(rps=('requests_per_window',pl.mean))

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
df = joined_table[joined_table['service'] != '']
pl.display(df)
)pxl";

TEST_F(LogicalPlannerTest, distributed_plan_test_basic_queries) {
  auto planner = LogicalPlanner::Create(false).ConsumeValueOrDie();
  auto plan_or_s = planner->Plan(distributedpb::testutils::CreateTwoAgentsOneKelvinPlannerState(
                                     distributedpb::testutils::kHttpEventsSchema),
                                 kHttpRequestStats);
  EXPECT_OK(plan_or_s);
}

const char* kCompileTimeQuery = R"pxl(
t1 = pl.DataFrame(table='http_events', start_time='-120s')

t1['service'] = t1.attr['service']
t1['http_resp_latency_ms'] = t1['http_resp_latency_ns'] / 1.0E6
t1['failure'] = t1['http_resp_status'] >= 400
t1['range_group'] = t1['time_'] - pl.modulo(t1['time_'], 2000000000)
t1['s'] = pl.bin(t1['time_'],pl.seconds(3))

quantiles_agg = t1.groupby('service').agg(
  latency_quantiles=('http_resp_latency_ms', pl.quantiles),
  errors=('failure', pl.mean),
  throughput_total=('http_resp_status', pl.count),
)

quantiles_agg['latency_p50'] = pl.pluck(quantiles_agg['latency_quantiles'], 'p50')
quantiles_agg['latency_p90'] = pl.pluck(quantiles_agg['latency_quantiles'], 'p90')
quantiles_agg['latency_p99'] = pl.pluck(quantiles_agg['latency_quantiles'], 'p99')
quantiles_table = quantiles_agg[['service', 'latency_p50', 'latency_p90', 'latency_p99', 'errors', 'throughput_total']]

# The Range aggregate to calcualte the requests per second.
requests_agg = t1.groupby(['service', 'range_group']).agg(
  requests_per_window=('http_resp_status', pl.count),
)

rps_table = requests_agg.groupby('service').agg(rps=('requests_per_window',pl.mean))

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
joined_table['asid'] = pl.asid()
joined_table = joined_table[joined_table['service'] != '' and pl.asid() == 3870]
pl.display(joined_table)
)pxl";

TEST_F(LogicalPlannerTest, duplicate_int) {
  auto planner = LogicalPlanner::Create(false).ConsumeValueOrDie();
  auto plan_or_s = planner->Plan(distributedpb::testutils::CreateTwoAgentsPlannerState(
                                     distributedpb::testutils::kHttpEventsSchema),
                                 kCompileTimeQuery);
  EXPECT_OK(plan_or_s);
}

const char* kTwoWindowQuery = R"query(
t1 = pl.DataFrame(table='http_events', start_time='-300s')
t1['service'] = t1.attr['service']
t1['http_resp_latency_ms'] = t1['http_resp_latency_ns'] / 1.0E6
# edit this to increase/decrease window. Dont go lower than 1 second.
t1['window1'] = pl.bin(t1['time_'], pl.seconds(10))
t1['window2'] = pl.bin(t1['time_'] + pl.seconds(5), pl.seconds(10))
# groupby 1sec intervals per window
window1_agg = t1.groupby(['service', 'window1']).agg(
  quantiles=('http_resp_latency_ms', pl.quantiles),
)
window1_agg['p50'] = pl.pluck(window1_agg['quantiles'], 'p50')
window1_agg['p90'] = pl.pluck(window1_agg['quantiles'], 'p90')
window1_agg['p99'] = pl.pluck(window1_agg['quantiles'], 'p99')
window1_agg['time_'] = window1_agg['window1']
# window1_agg = window1_agg.drop('window1')

window2_agg = t1.groupby(['service', 'window2']).agg(
  quantiles=('http_resp_latency_ms', pl.quantiles),
)
window2_agg['p50'] = pl.pluck(window2_agg['quantiles'], 'p50')
window2_agg['p90'] = pl.pluck(window2_agg['quantiles'], 'p90')
window2_agg['p99'] = pl.pluck(window2_agg['quantiles'], 'p99')
window2_agg['time_'] = window2_agg['window2']
# window2_agg = window2_agg.drop('window2')

df = window2_agg[window2_agg['service'] != '']
pl.display(df)
)query";
TEST_F(LogicalPlannerTest, NestedCompileTime) {
  auto planner = LogicalPlanner::Create(false).ConsumeValueOrDie();
  auto plan_or_s = planner->Plan(distributedpb::testutils::CreateTwoAgentsPlannerState(
                                     distributedpb::testutils::kHttpEventsSchema),
                                 kTwoWindowQuery);
  EXPECT_OK(plan_or_s);
}
}  // namespace logical_planner
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
