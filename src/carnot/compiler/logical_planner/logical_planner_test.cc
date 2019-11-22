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

// Future test when we actually start using kelvin nodes in the system (dependent upon a later
// diff (PL-873)).
// TODO(zasgar) (PL-873) enable test and disable/remove many_agents test.
TEST_F(LogicalPlannerTest, DISABLED_two_agents_one_kelvin) {
  auto planner = LogicalPlanner::Create().ConsumeValueOrDie();
  auto plan = planner
                  ->Plan(distributedpb::testutils::CreateTwoAgentsOneKelvinPlannerState(),
                         distributedpb::testutils::kQueryForTwoAgents)
                  .ConsumeValueOrDie();
  auto out_pb = plan->ToProto().ConsumeValueOrDie();
  EXPECT_THAT(out_pb,
              Partially(EqualsProto(distributedpb::testutils::kExpectedPlanTwoAgentOneKelvin)));
}

TEST_F(LogicalPlannerTest, many_agents) {
  auto planner = LogicalPlanner::Create().ConsumeValueOrDie();
  auto plan = planner
                  ->Plan(distributedpb::testutils::CreateTwoAgentsPlannerState(),
                         distributedpb::testutils::kQueryForTwoAgents)
                  .ConsumeValueOrDie();
  auto out_pb = plan->ToProto().ConsumeValueOrDie();
  EXPECT_THAT(out_pb, Partially(EqualsProto(distributedpb::testutils::kExpectedPlanTwoAgents)));
}

// TODO(nserrino): Add service metadata column back in (see commented code)
const char* kHttpRequestStats = R"pxl(
t1 = dataframe(table='http_events').range(start='-30s')

t1['http_resp_latency_ms'] = t1['http_resp_latency_ns'] / 1.0E6
t1['failure'] = t1['http_resp_status'] >= 400
t1['range_group'] = pl.subtract(t1['time_'], pl.modulo(t1['time_'], 1000000000))

quantiles_agg = t1.agg(by=lambda r: [r.attr.service], fn=lambda r: {
  'latency_quantiles': pl.quantiles(r.http_resp_latency_ms),
  'errors': pl.mean(r.failure),
  'throughput_total': pl.count(r.http_resp_status),
})

quantiles_agg['latency_p50'] = pl.pluck(quantiles_agg['latency_quantiles'], 'p50')
quantiles_agg['latency_p90'] = pl.pluck(quantiles_agg['latency_quantiles'], 'p90')
quantiles_agg['latency_p99'] = pl.pluck(quantiles_agg['latency_quantiles'], 'p99')
# quantiles_agg['service'] = quantiles_agg.attr['service']
quantiles_agg['service'] = 'foo'
quantiles_table = quantiles_agg[['service', 'latency_p50', 'latency_p90', 'latency_p99', 'errors', 'throughput_total']]

# The Range aggregate to calcualte the requests per second.
range_agg = t1.agg(by=lambda r: [r.attr.service, r.range_group], fn=lambda r: {
  'requests_per_window': pl.count(r.http_resp_status)
})

rps_table = range_agg.agg(by=lambda r: r.attr.service, fn= lambda r: {'rps': pl.mean(r.requests_per_window)})
joined_table = quantiles_table.merge(rps_table,  type='inner',
                                    cond=lambda r1, r2: r1.service == r2._attr_service_name,
                                    cols=lambda r1, r2: {
                                      "service" : r1.service,
                                      'latency(p50)': r1.latency_p50,
                                      'latency(p90)': r1.latency_p90,
                                      'latency(p99)': r1.latency_p99,
                                      'errors': r1.errors,
                                      'throughput (rps)': r2.rps,
                                      'throughput total': r1.throughput_total,
                                    })
joined_table.filter(fn=lambda r: r.service != "").result(name="out")

)pxl";

TEST_F(LogicalPlannerTest, distributed_plan_test_basic_queries) {
  auto planner = LogicalPlanner::Create().ConsumeValueOrDie();
  auto plan_or_s = planner->Plan(distributedpb::testutils::CreateTwoAgentsOneKelvinPlannerState(
                                     distributedpb::testutils::kHttpEventsSchema),
                                 kHttpRequestStats);
  EXPECT_OK(plan_or_s);
}

}  // namespace logical_planner
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
