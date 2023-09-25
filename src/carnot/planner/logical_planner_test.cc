/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/api/proto/uuidpb/uuid.pb.h"
#include "src/carnot/planner/compiler/graph_comparison.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/distributed_planner.h"
#include "src/carnot/planner/distributedpb/distributed_plan.pb.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/logical_planner.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/planner/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/protobuf.h"
#include "src/common/testing/status.h"

namespace px {
namespace carnot {
namespace planner {
using ::px::carnot::planner::testing::EqualsPlanGraph;
using ::px::testing::proto::EqualsProto;

class LogicalPlannerTest : public ::testing::Test {
 protected:
  void SetUp() { info_ = udfexporter::ExportUDFInfo().ConsumeValueOrDie()->info_pb(); }
  plannerpb::QueryRequest MakeQueryRequest(const distributedpb::LogicalPlannerState& state,
                                           const std::string& query) {
    plannerpb::QueryRequest query_request;
    query_request.set_query_str(query);
    *query_request.mutable_logical_planner_state() = state;
    return query_request;
  }
  udfspb::UDFInfo info_;
};

TEST_F(LogicalPlannerTest, one_pems_one_kelvin) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  ASSERT_OK_AND_ASSIGN(auto plan, planner->Plan(MakeQueryRequest(
                                      testutils::CreateOnePEMOneKelvinPlannerState(),
                                      "import px\npx.display(px.DataFrame('table1'), 'out')")));
  auto plan_pb = plan->ToProto().ConsumeValueOrDie();
  distributedpb::DistributedPlan expected_pb;
  google::protobuf::TextFormat::MergeFromString(testutils::kExpectedPlanOnePEMOneKelvin,
                                                &expected_pb);

  auto kelvin_plan = plan_pb.qb_address_to_plan().find("kelvin");
  EXPECT_THAT(kelvin_plan->second,
              EqualsPlanGraph(expected_pb.qb_address_to_plan().find("kelvin")->second));
  ASSERT_NE(kelvin_plan, plan_pb.qb_address_to_plan().end());
  EXPECT_EQ(kelvin_plan->second.execution_status_destinations_size(), 1);
  EXPECT_EQ(kelvin_plan->second.execution_status_destinations()[0].grpc_address(),
            "query-broker-ip:50300");
  EXPECT_EQ(kelvin_plan->second.execution_status_destinations()[0].ssl_targetname(),
            "query-broker-hostname");

  auto pem_plan = plan_pb.qb_address_to_plan().find("pem");
  EXPECT_THAT(pem_plan->second,
              EqualsPlanGraph(expected_pb.qb_address_to_plan().find("pem")->second));
  ASSERT_NE(pem_plan, plan_pb.qb_address_to_plan().end());
  EXPECT_EQ(pem_plan->second.execution_status_destinations_size(), 1);
  EXPECT_EQ(pem_plan->second.execution_status_destinations()[0].grpc_address(), "1111");
  EXPECT_EQ(pem_plan->second.execution_status_destinations()[0].ssl_targetname(), "kelvin.pl.svc");
}

TEST_F(LogicalPlannerTest, distributed_plan_test_basic_queries) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  auto ps = testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema);
  ASSERT_OK_AND_ASSIGN(auto plan,
                       planner->Plan(MakeQueryRequest(ps, testutils::kHttpRequestStats)));
  ASSERT_OK_AND_ASSIGN(auto plan_pb, plan->ToProto());

  auto kelvin_plan = plan_pb.qb_address_to_plan().find("kelvin");
  ASSERT_NE(kelvin_plan, plan_pb.qb_address_to_plan().end());
  EXPECT_EQ(kelvin_plan->second.execution_status_destinations_size(), 1);
  EXPECT_EQ(kelvin_plan->second.execution_status_destinations()[0].grpc_address(),
            "query-broker-ip:50300");
  EXPECT_EQ(kelvin_plan->second.execution_status_destinations()[0].ssl_targetname(),
            "query-broker-hostname");

  auto pem1_plan = plan_pb.qb_address_to_plan().find("pem1");
  ASSERT_NE(pem1_plan, plan_pb.qb_address_to_plan().end());
  EXPECT_EQ(pem1_plan->second.execution_status_destinations_size(), 1);
  EXPECT_EQ(pem1_plan->second.execution_status_destinations()[0].grpc_address(), "1111");
  EXPECT_EQ(pem1_plan->second.execution_status_destinations()[0].ssl_targetname(), "kelvin.pl.svc");
}

constexpr char kSimpleQueryDefaultLimit[] = R"pxl(
import px
t1 = px.DataFrame(table='http_events', start_time='-120s', select=['time_'])
px.display(t1)
)pxl";

TEST_F(LogicalPlannerTest, max_output_rows) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  auto state = testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema);
  state.mutable_plan_options()->set_max_output_rows_per_table(100);
  auto plan_or_s = planner->Plan(MakeQueryRequest(state, kSimpleQueryDefaultLimit));
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
import px

t1 = px.DataFrame(table='http_events', start_time='-120s')

t1['service'] = t1.ctx['service']
t1['http_resp_latency_ms'] = t1['resp_latency_ns'] / 1.0E6
t1['failure'] = t1['resp_status'] >= 400
t1['range_group'] = t1['time_'] - px.modulo(t1['time_'], 2000000000)
t1['s'] = px.bin(t1['time_'],px.seconds(3))

quantiles_agg = t1.groupby('service').agg(
  latency_quantiles=('http_resp_latency_ms', px.quantiles),
  errors=('failure', px.mean),
  throughput_total=('resp_status', px.count),
)

quantiles_agg['latency_p50'] = px.pluck(quantiles_agg['latency_quantiles'], 'p50')
quantiles_agg['latency_p90'] = px.pluck(quantiles_agg['latency_quantiles'], 'p90')
quantiles_agg['latency_p99'] = px.pluck(quantiles_agg['latency_quantiles'], 'p99')
quantiles_table = quantiles_agg[['service', 'latency_p50', 'latency_p90', 'latency_p99', 'errors', 'throughput_total']]

# The Range aggregate to calcualte the requests per second.
requests_agg = t1.groupby(['service', 'range_group']).agg(
  requests_per_window=('resp_status', px.count),
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
  auto plan_or_s = planner->Plan(
      MakeQueryRequest(testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema),
                       kCompileTimeQuery));
  EXPECT_OK(plan_or_s);
  auto plan = plan_or_s.ConsumeValueOrDie();
  EXPECT_OK(plan->ToProto());
}

constexpr char kTwoWindowQuery[] = R"query(
import px

t1 = px.DataFrame(table='http_events', start_time='-300s')
t1['service'] = t1.ctx['service']
t1['http_resp_latency_ms'] = t1['resp_latency_ns'] / 1.0E6
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
  auto plan_or_s = planner->Plan(
      MakeQueryRequest(testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema),
                       kTwoWindowQuery));
  EXPECT_OK(plan_or_s);
  auto plan = plan_or_s.ConsumeValueOrDie();
  EXPECT_OK(plan->ToProto());
}

constexpr char kAppendQuery[] = R"pxl(
import px

df1 = px.DataFrame(table='http_events', start_time='-2m', select=['time_', 'upid'])
df2 = px.DataFrame(table='http_events', start_time='-3m', select=['time_', 'upid'])
px.display(df1.append(df2))
)pxl";

TEST_F(LogicalPlannerTest, AppendTest) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  auto plan_or_s = planner->Plan(MakeQueryRequest(
      testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema), kAppendQuery));
  EXPECT_OK(plan_or_s);
  auto plan = plan_or_s.ConsumeValueOrDie();
  EXPECT_OK(plan->ToProto());
}

constexpr char kAppendSelfQuery[] = R"pxl(
import px

df1 = px.DataFrame(table='http_events', start_time='-5m', select=['time_', 'upid'])
df2 = df1[2==2]
px.display(df1.append(df2))
)pxl";

TEST_F(LogicalPlannerTest, AppendSelfTest) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  auto plan_or_s = planner->Plan(
      MakeQueryRequest(testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema),
                       kAppendSelfQuery));
  EXPECT_OK(plan_or_s);
  auto plan = plan_or_s.ConsumeValueOrDie();
  EXPECT_OK(plan->ToProto());
}

constexpr char kPlannerQueryError[] = R"pxl(
import px

###############################################################
# Edit the following variables to change the visualization.
###############################################################
# Pods/services are formatted as <namespace>/<name>.
# If you want to match a namespace, only keep the namespace portion
match_name = 'sock-shop/order'
k8s_object = 'service'
requestor_filter = ''  # 'front-end'
# Visualization Variables - Dont change unless you know what you are doing
num_seconds = 2
filter_dash = True
filter_health = True
filter_readyz = True
filter_empty_k8s = True
src_name = 'requestor'
dest_name = 'responder'
ip = 'remote_addr'
###############################################################
df = px.DataFrame(table='http_events', start_time='-2m')
df.http_resp_latency_ms = df.resp_latency_ns / 1.0E6
df = df[df['http_resp_latency_ms'] < 1000.0]
df.failure = df.resp_status >= 400
df.timestamp = px.bin(df.time_, px.seconds(num_seconds))
df[k8s_object] = df.ctx[k8s_object]
filter_pods = px.contains(df[k8s_object], match_name)
filter_out_conds = ((df.req_path != '/health' or not filter_health) and (
    df.req_path != '/readyz' or not filter_readyz)) and (
    df[ip] != '-' or not filter_dash)

filt_df = df[filter_out_conds]
qa = filt_df[filter_pods]
qa = qa.groupby([k8s_object, 'timestamp']).agg(
    latency_quantiles=('http_resp_latency_ms', px.quantiles),
    error_rate_per_window=('failure', px.mean),
    throughput_total=('resp_status', px.count),
)
qa.latency_p50 = px.pluck_float64(qa.latency_quantiles, 'p50')
qa.latency_p90 = px.pluck_float64(qa.latency_quantiles, 'p90')
qa.latency_p99 = px.pluck_float64(qa.latency_quantiles, 'p99')
qa['time_'] = qa['timestamp']
qa.error_rate = qa.error_rate_per_window * qa.throughput_total / num_seconds
qa.rps = qa.throughput_total / num_seconds
qa['k8s'] = qa[k8s_object]
px.display(qa['time_', 'k8s', 'latency_p50',
              'latency_p90', 'latency_p99', 'error_rate', 'rps'], 'test')

##### Map which services this talks to.
df = filt_df.groupby([k8s_object, ip]).agg(count=(ip, px.count))
df['pod_id'] = px.ip_to_pod_id(df[ip])
# # Enable if you want pod name
# df[src_name] = px.pod_id_to_pod_name(df.pod_id)
df[src_name] = px.pod_id_to_service_name(df.pod_id)
df[dest_name] = df[k8s_object]

res = df[px.contains(df[dest_name], match_name)]
res = res.groupby([src_name, dest_name]).agg(count=(src_name, px.count))
px.display(res[[src_name, dest_name]], 'receives_from')

rcv = df[px.contains(df[src_name], match_name)]
rcv = rcv.groupby([src_name, dest_name]).agg(count=(src_name, px.count))
px.display(rcv[[src_name, dest_name]], 'talks_to')
)pxl";

TEST_F(LogicalPlannerTest, BrokenQueryTest) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  auto plan_or_s = planner->Plan(
      MakeQueryRequest(testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema),
                       kPlannerQueryError));
  ASSERT_OK(plan_or_s);
  auto plan = plan_or_s.ConsumeValueOrDie();
  EXPECT_OK(plan->ToProto());
}

constexpr char kPlanExecFuncs[] = R"pxl(
import px
def f(a: int):
  return px.DataFrame('http_events', start_time='-2m')
)pxl";

TEST_F(LogicalPlannerTest, PlanWithExecFuncs) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  plannerpb::QueryRequest req;
  req.set_query_str(kPlanExecFuncs);
  auto f = req.add_exec_funcs();
  f->set_func_name("f");
  f->set_output_table_prefix("test");
  auto a = f->add_arg_values();
  a->set_name("a");
  a->set_value("1");

  *req.mutable_logical_planner_state() =
      testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema);
  auto plan_or_s = planner->Plan(req);
  ASSERT_OK(plan_or_s);
  auto plan = plan_or_s.ConsumeValueOrDie();
  EXPECT_OK(plan->ToProto());
}

constexpr char kBPFTraceProgramMaxKernel[] = R"bpftrace(
kprobe:tcp_drop
{
  ...
}
)bpftrace";

constexpr char kBPFTraceProgramMinKernel[] = R"bpftrace(
tracepoint:skb:kfree_skb
{
  ...
}
)bpftrace";

constexpr char kTwoTraceProgramsPxl[] = R"pxl(
import pxtrace
import px

before_518_trace_program = pxtrace.TraceProgram(
  program="""$0""",
  max_kernel='5.18',
)

after_519_trace_program = pxtrace.TraceProgram(
  program="""$1""",
  min_kernel='5.19',
)

table_name = 'tcp_drop_table'
pxtrace.UpsertTracepoint('tcp_drop_tracer',
                          table_name,
                          [before_518_trace_program, after_519_trace_program],
                          pxtrace.kprobe(),
                          '10m')
)pxl";

constexpr char kBPFTwoTraceProgramsPb[] = R"proto(
name: "tcp_drop_tracer"
ttl {
  seconds: 600
}
programs {
  table_name: "tcp_drop_table"
  bpftrace {
    program: "\nkprobe:tcp_drop\n{\n  ...\n}\n"
  }
  selectors {
    selector_type: MAX_KERNEL
    value: "5.18"
  }
}
programs {
  table_name: "tcp_drop_table"
  bpftrace {
    program: "\ntracepoint:skb:kfree_skb\n{\n  ...\n}\n"
  }
  selectors {
    selector_type: MIN_KERNEL
    value: "5.19"
  }
}
)proto";

TEST_F(LogicalPlannerTest, CompileTwoTracePrograms) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  plannerpb::CompileMutationsRequest req;
  req.set_query_str(
      absl::Substitute(kTwoTraceProgramsPxl, kBPFTraceProgramMaxKernel, kBPFTraceProgramMinKernel));
  *req.mutable_logical_planner_state() =
      testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema);
  auto trace_ir_or_s = planner->CompileTrace(req);
  ASSERT_OK(trace_ir_or_s);
  auto trace_ir = trace_ir_or_s.ConsumeValueOrDie();
  plannerpb::CompileMutationsResponse resp;
  ASSERT_OK(trace_ir->ToProto(&resp));
  ASSERT_EQ(resp.mutations_size(), 1);
  EXPECT_THAT(resp.mutations()[0].trace(), EqualsProto(kBPFTwoTraceProgramsPb));
}

constexpr char kSingleProbePxl[] = R"pxl(
import pxtrace
import px

@pxtrace.probe("MyFunc")
def probe_func():
    id = pxtrace.ArgExpr('id')
    return [{'id': id},
            {'err': pxtrace.RetExpr('$0.a')},
            {'latency': pxtrace.FunctionLatency()}]

pxtrace.UpsertTracepoint('http_return',
                         'http_return_table',
                         probe_func,
                         px.uint128("123e4567-e89b-12d3-a456-426655440000"),
                         "5m")
)pxl";

constexpr char kSingleProbeProgramPb[] = R"proto(
name: "http_return"
ttl {
  seconds: 300
}
deployment_spec {
  upid {
    asid: 306070887 pid: 3902477011 ts_ns: 11841725277501915136
  }
}
programs {
  table_name: "http_return_table"
  spec {
    outputs {
      name: "http_return_table"
      fields: "id"
      fields: "err"
      fields: "latency"
    }
    probe {
      name: "http_return"
      tracepoint {
        symbol: "MyFunc"
      }
      args {
        id: "arg0"
        expr: "id"
      }
      ret_vals {
        id: "ret0"
        expr: "$0.a"
      }
      function_latency {
        id: "lat0"
      }
      output_actions {
        output_name: "http_return_table"
        variable_names: "arg0"
        variable_names: "ret0"
        variable_names: "lat0"
      }
    }
  }
}
)proto";

TEST_F(LogicalPlannerTest, CompileTrace) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  plannerpb::CompileMutationsRequest req;
  req.set_query_str(kSingleProbePxl);
  *req.mutable_logical_planner_state() =
      testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema);
  auto trace_ir_or_s = planner->CompileTrace(req);
  ASSERT_OK(trace_ir_or_s);
  auto trace_ir = trace_ir_or_s.ConsumeValueOrDie();
  plannerpb::CompileMutationsResponse resp;
  ASSERT_OK(trace_ir->ToProto(&resp));
  ASSERT_EQ(resp.mutations_size(), 1);
  EXPECT_THAT(resp.mutations()[0].trace(), EqualsProto(kSingleProbeProgramPb));
}

constexpr char kSingleProbeInFuncPxl[] = R"pxl(
import pxtrace
import px

@pxtrace.probe("MyFunc")
def probe_func():
    id = pxtrace.ArgExpr('id')
    return [{'id': id},
            {'err': pxtrace.RetExpr('$0.a')},
            {'latency': pxtrace.FunctionLatency()}]

def probe_table(upid: str):
  pxtrace.UpsertTracepoint('http_return',
                           'http_return_table',
                           probe_func,
                           px.uint128(upid),
                           '5m')
  return px.DataFrame('http_return_table')

)pxl";

TEST_F(LogicalPlannerTest, CompileTraceWithExecFuncs) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  plannerpb::CompileMutationsRequest req;
  req.set_query_str(kSingleProbeInFuncPxl);
  auto func_to_execute = req.add_exec_funcs();
  func_to_execute->set_func_name("probe_table");
  func_to_execute->set_output_table_prefix("output");
  auto duration = func_to_execute->add_arg_values();
  duration->set_name("upid");
  duration->set_value("123e4567-e89b-12d3-a456-426655440000");
  *req.mutable_logical_planner_state() =
      testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema);

  auto trace_ir_or_s = planner->CompileTrace(req);
  ASSERT_OK(trace_ir_or_s);
  auto trace_ir = trace_ir_or_s.ConsumeValueOrDie();
  plannerpb::CompileMutationsResponse resp;
  ASSERT_OK(trace_ir->ToProto(&resp));
  ASSERT_EQ(resp.mutations_size(), 1);
  EXPECT_THAT(resp.mutations()[0].trace(), EqualsProto(kSingleProbeProgramPb));
}
constexpr char kBrokenFunc1234[] = R"pxl(
''' HTTP Data Tracer
This script traces all HTTP/HTTP2 data on the cluster for a specified amount of time.
An optional filter prints only those traces that include the specified service name.
'''
import px
# ----------------------------------------------------------------
# Script variables
# ----------------------------------------------------------------
service_matcher = ''
start_time = '-30s'
max_num_records = 100
# ----------------------------------------------------------------
# Implementation
# ----------------------------------------------------------------
df = px.DataFrame(table='http_events', select=['time_', 'upid', 'remote_addr', 'remote_port',
                                               'req_method', 'req_path',
                                               'resp_status', 'resp_message',
                                               'resp_body',
                                               'resp_latency_ns'], start_time=start_time)
df2 = df.agg(c=('resp_body', px.count))
px.display(df2)
)pxl";
TEST_F(LogicalPlannerTest, partial_agg) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  auto plan_or_s = planner->Plan(
      MakeQueryRequest(testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema),
                       kBrokenFunc1234));
  EXPECT_OK(plan_or_s);
  auto plan = plan_or_s.ConsumeValueOrDie();
  EXPECT_OK(plan->ToProto());
}

constexpr char kPemOnlyLimit[] = R"pxl(
import px
df = px.DataFrame(table='http_events')
df = df.head(n=100, _pem_only=1)
df.test = 1
px.display(df)
)pxl";
TEST_F(LogicalPlannerTest, pem_only_limit) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  auto plan_or_s = planner->Plan(MakeQueryRequest(
      testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema), kPemOnlyLimit));
  EXPECT_OK(plan_or_s);
  auto plan = plan_or_s.ConsumeValueOrDie();
  EXPECT_OK(plan->ToProto());
}

constexpr char kLimitFailing[] = R"pxl(
import pxtrace
import px


# func Sum(l, r pb.Money) (pb.Money, error)
@pxtrace.probe('github.com/GoogleCloudPlatform/microservices-demo/src/checkoutservice/money.Sum')
def probe_func():
    return [{'l': pxtrace.ArgExpr('l')},
            {'r': pxtrace.ArgExpr('r')},
            {'result': pxtrace.RetExpr('$0')},
            {'error': pxtrace.RetExpr('$1')},
            {'latency': pxtrace.FunctionLatency()}]


table_name = 'checkout_table1'
trace_name = 'checkout_probe1'

# Change to the Pod you want to trace.
pod_name = 'online-boutique/checkoutservice'

pxtrace.UpsertTracepoint(trace_name, table_name,
                         probe_func,
                         pxtrace.PodProcess(pod_name),
                         ttl='10m')

df = px.DataFrame(table_name)
# nil interface have both 'tab' and 'data' being 0, which means the function finishes successfully.
df_success = df[px.pluck(df.error, 'data') == '0']
df_success.error = 'nil'
px.display(df_success)

df_failed = df[px.pluck(df.error, 'data') != '0']
df_failed.error = px.pluck_int64(df.error, 'code')

# Error code 13 means invalid value.
df_failed_13 = df_failed[df_failed.error == 13]
df_failed_13.error = "Invalid value"

df_failed_other = df_failed[df_failed.error != 13]
df_failed_other.error = "Other"

df_failed_13
# Concatenate 2 parts to form the full list.
px.display(df_failed_13.append(df_failed_other))

)pxl";

constexpr char kCheckoutProbeTableSchema[] = R"proto(
relation_map {
  key: "checkout_table1"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "goid_"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "l"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "r"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "result"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "error"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "latency"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
  }
}
)proto";
TEST_F(LogicalPlannerTest, limit_pushdown_failing) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  auto state = testutils::CreateTwoPEMsOneKelvinPlannerState(kCheckoutProbeTableSchema);
  // Replicate what happens in the main environment.
  state.mutable_plan_options()->set_max_output_rows_per_table(10000);

  auto plan_or_s = planner->Plan(MakeQueryRequest(state, kLimitFailing));
  EXPECT_OK(plan_or_s);
  auto plan = plan_or_s.ConsumeValueOrDie();
  auto proto_or_s = plan->ToProto();
  ASSERT_OK(proto_or_s.status());
}

const char kFilterPushDownBugQuery[] = R"pxl(
import px

df = px.DataFrame(table='http_events', start_time='-6m')
df.service = df.ctx['service']

df.requestor_pod_id = px.ip_to_pod_id(df.remote_addr)
df.responder_service = df.service
df.requestor_service = px.pod_id_to_service_name(df.requestor_pod_id)
df = df.groupby(['responder_service', 'requestor_service']).agg()

df = df[df.requestor_service != '' and df.responder_service != '']

px.display(df)
)pxl";
TEST_F(LogicalPlannerTest, filter_pushdown_bug) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  auto state = testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema);
  ASSERT_OK_AND_ASSIGN(auto plan, planner->Plan(MakeQueryRequest(state, kFilterPushDownBugQuery)));
  ASSERT_OK(plan->ToProto());
}

const char kHttpDataScript[] = R"pxl(
import px


def http_data(start_time: str, source_filter: str, destination_filter: str, num_head: int):

    df = px.DataFrame(table='http_events', start_time=start_time)

    # Add context.
    df.node = df.ctx['node']
    df.pid = px.upid_to_pid(df.upid)

    # Filter out entities as specified by the user.
    df = df[px.contains("source", source_filter)]
    df = df[px.contains("destination", destination_filter)]

    # Add additional filters below:

    # Restrict number of results.
    df = df.head(num_head)

    # Order columns.

    return df
)pxl";

TEST_F(LogicalPlannerTest, VerifyEmptyContainsCallsDoNotSegFaultTest) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  plannerpb::QueryRequest req;
  req.set_query_str(kHttpDataScript);
  auto f = req.add_exec_funcs();
  f->set_func_name("http_data");
  f->set_output_table_prefix("http_data");
  auto start_time = f->add_arg_values();
  start_time->set_name("start_time");
  start_time->set_value("-5m");

  auto source_filter = f->add_arg_values();
  source_filter->set_name("source_filter");
  source_filter->set_value("");

  auto dest_filter = f->add_arg_values();
  dest_filter->set_name("destination_filter");
  dest_filter->set_value("");

  auto num_head = f->add_arg_values();
  num_head->set_name("num_head");
  num_head->set_value("1000");

  *req.mutable_logical_planner_state() =
      testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema);
  auto plan_or_s = planner->Plan(req);
  ASSERT_OK(plan_or_s);
  auto plan = plan_or_s.ConsumeValueOrDie();
  EXPECT_OK(plan->ToProto());
}

TEST_F(LogicalPlannerTest, create_compiler_state_has_endpoint_config) {
  auto state = testutils::CreateTwoPEMsOneKelvinPlannerState(kCheckoutProbeTableSchema);
  auto endpoint_config = state.mutable_otel_endpoint_config();
  endpoint_config->set_url("px.dev:55555");
  (*endpoint_config->mutable_headers())["key1"] = "value1";
  (*endpoint_config->mutable_headers())["key2"] = "value2";
  planner::RegistryInfo registry_info;
  ASSERT_OK(registry_info.Init(info_));
  ASSERT_OK_AND_ASSIGN(auto compiler_state, CreateCompilerState(state, &registry_info,
                                                                /* max_output_rows_per_table*/ 0));
  EXPECT_EQ(compiler_state->endpoint_config()->url(), "px.dev:55555");
  EXPECT_EQ(compiler_state->endpoint_config()->headers().size(), 2);
  EXPECT_EQ(compiler_state->endpoint_config()->headers().at("key1"), "value1");
  EXPECT_EQ(compiler_state->endpoint_config()->headers().at("key2"), "value2");
}

TEST_F(LogicalPlannerTest, default_compiler_state_has_nullptr) {
  auto state = testutils::CreateTwoPEMsOneKelvinPlannerState(kCheckoutProbeTableSchema);
  planner::RegistryInfo registry_info;
  ASSERT_OK(registry_info.Init(info_));
  ASSERT_OK_AND_ASSIGN(auto compiler_state, CreateCompilerState(state, &registry_info,
                                                                /* max_output_rows_per_table*/ 0));
  EXPECT_EQ(compiler_state->endpoint_config(), nullptr);
}

const char kOTelDebugInfo[] = R"pxl(
import px

df = px.DataFrame(table='http_events', start_time='-6m')
df.service = df.ctx['service']
px.export(df, px.otel.Data(
  endpoint=px.otel.Endpoint(url="px.dev:55555"),
  resource={
      'service.name' : df.service,
  },
  data=[
    px.otel.metric.Gauge(
      name='resp_latency',
      value=df.resp_latency_ns,
    )
  ]
))
)pxl";

TEST_F(LogicalPlannerTest, otel_debug_attributes_end_to_end) {
  auto state = testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema);
  auto debug_info = state.mutable_debug_info();
  auto attr1 = debug_info->add_otel_debug_attributes();
  attr1->set_name("pixie_cloud");
  attr1->set_value("work.dev.px.dev");
  auto attr2 = debug_info->add_otel_debug_attributes();
  attr2->set_name("pixie_version");
  attr2->set_value("v1.2.3");
  planner::RegistryInfo registry_info;
  ASSERT_OK(registry_info.Init(info_));
  ASSERT_OK_AND_ASSIGN(auto compiler_state, CreateCompilerState(state, &registry_info,
                                                                /* max_output_rows_per_table*/ 0));

  EXPECT_EQ(compiler_state->debug_info().otel_debug_attrs[0].name, "pixie_cloud");
  EXPECT_EQ(compiler_state->debug_info().otel_debug_attrs[0].value, "work.dev.px.dev");
  EXPECT_EQ(compiler_state->debug_info().otel_debug_attrs[1].name, "pixie_version");
  EXPECT_EQ(compiler_state->debug_info().otel_debug_attrs[1].value, "v1.2.3");

  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  ASSERT_OK_AND_ASSIGN(auto plan, planner->Plan(MakeQueryRequest(state, kOTelDebugInfo)));
  ASSERT_OK_AND_ASSIGN(auto distributed_plan, plan->ToProto());
  auto kelvin_plan = (*distributed_plan.mutable_qb_address_to_plan())["kelvin"];

  planpb::OTelExportSinkOperator operator_proto;
  int64_t count = 0;
  for (const auto& planFragment : kelvin_plan.nodes()) {
    for (const auto& planNode : planFragment.nodes()) {
      if (planNode.op().op_type() == planpb::OperatorType::OTEL_EXPORT_SINK_OPERATOR) {
        operator_proto = planNode.op().otel_sink_op();
        ++count;
      }
    }
  }
  EXPECT_EQ(count, 1);

  EXPECT_THAT(operator_proto.resource(), EqualsProto(R"proto(
attributes {
  name: "service.name"
  column {
    column_type: STRING
    column_index: 2
    can_be_json_encoded_array: true
  }
}
attributes {
  name: "pixie_cloud"
  string_value: "work.dev.px.dev"
}
attributes {
  name: "pixie_version"
  string_value: "v1.2.3"
})proto"));
}

TEST_F(LogicalPlannerTest, GenerateOTelScript) {
  auto planner = LogicalPlanner::Create(info_).ConsumeValueOrDie();
  auto state = testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema);
  plannerpb::GenerateOTelScriptRequest req;
  *req.mutable_logical_planner_state() = state;
  req.set_pxl_script(R"pxl(import px
df = px.DataFrame('http_events', start_time='-5m')
df.service = df.ctx['service']
df = df[['time_', 'service', 'resp_latency_ns']]
px.display(df, 'http_graph'))pxl");
  ASSERT_OK_AND_ASSIGN(auto resp, planner->GenerateOTelScript(req));
  EXPECT_EQ(resp->otel_script(), R"otel(import px
df = px.DataFrame('http_events', start_time=px.plugin.start_time, end_time=px.plugin.end_time)
df.service = df.ctx['service']
df = df[['time_', 'service', 'resp_latency_ns']]
px.display(df, 'http_graph')

otel_df = df
px.export(otel_df, px.otel.Data(
  resource={
    'http_graph.service': otel_df.service,
    'service.name': otel_df.service
  },
  data=[
    px.otel.metric.Gauge(
      name='http_graph.resp_latency_ns',
      description='',
      value=otel_df.resp_latency_ns,
    )
  ]
)))otel");
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
