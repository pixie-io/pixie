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

#include "src/carnot/planner/probes/probes.h"
#include "src/carnot/planner/compiler/ast_visitor.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {
using ::testing::ContainsRegex;
using ::testing::Not;
using ::testing::UnorderedElementsAre;

class ProbeCompilerTest : public ASTVisitorTest {
 protected:
  StatusOr<std::shared_ptr<compiler::MutationsIR>> CompileProbeScript(
      std::string_view query, const ExecFuncs& exec_funcs = {}) {
    absl::flat_hash_set<std::string> reserved_names;
    for (const auto& func : exec_funcs) {
      reserved_names.insert(func.output_table_prefix());
    }
    auto func_based_exec = exec_funcs.size() > 0;

    Parser parser;
    PX_ASSIGN_OR_RETURN(auto ast, parser.Parse(query));

    std::shared_ptr<IR> ir = std::make_shared<IR>();
    std::shared_ptr<compiler::MutationsIR> probe_ir = std::make_shared<compiler::MutationsIR>();

    ModuleHandler module_handler;
    PX_ASSIGN_OR_RETURN(auto ast_walker, compiler::ASTVisitorImpl::Create(
                                             ir.get(), probe_ir.get(), compiler_state_.get(),
                                             &module_handler, func_based_exec, reserved_names, {}));

    PX_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));
    if (func_based_exec) {
      PX_RETURN_IF_ERROR(ast_walker->ProcessExecFuncs(exec_funcs));
    }
    return probe_ir;
  }
};

constexpr char kSingleProbeUpsertPxlTpl[] = R"pxl(
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
                         $1,
                         '5m')
)pxl";

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

constexpr char kSingleProbeProgramOnUPIDPb[] = R"pxl(
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
)pxl";

constexpr char kUProbeBPFTraceUpsertPxlTpl[] = R"pxl(
import pxtrace
import px

program = """
uprobe:/bin/bash:readline { printf("arg0: %d\n", arg0) };
"""

table_name = 'bin_bash'
pxtrace.UpsertTracepoint('bin_bash_tracer',
                          table_name,
                          program,
                          $0,
                          "10m")
df = px.DataFrame(table=table_name)
)pxl";

constexpr char kLabelSelectorDeploymentSpec[] = R"pxl(
label_selector: {
  labels {
    key: "app"
    value: "querybroker"
  }
  namespace: "pl"
  container: "querybroker"
  process: "/app/querybroker"
}
)pxl";

constexpr char klabelSelectorDeploymentSpecDefaultNS[] = R"pxl(
label_selector: {
  labels {
    key: "app"
    value: "querybroker"
  }
  namespace: "default"
  container: "querybroker"
  process: "/app/querybroker"
}
)pxl";

constexpr char kPodProcessDeploymentSpec[] = R"pxl(
pod_process: {
  pods: "pl/vizier-query-broker-85dc9bc4d-jzw4s"
  container: "querybroker"
  process: "/app/querybroker"
}
)pxl";

constexpr char kPodProcessDeploymentSpecNoProcessName[] = R"pxl(
pod_process: {
  pods: "pl/vizier-query-broker-85dc9bc4d-jzw4s"
  container: "querybroker"
}
)pxl";

constexpr char kPodProcessDeploymentSpecJustPod[] = R"pxl(
pod_process: {
  pods: "pl/vizier-query-broker-85dc9bc4d-jzw4s"
}
)pxl";

constexpr char kSingleProbeUpsertSharedObjectProgramPb[] = R"pxl(
name: "http_return"
ttl {
  seconds: 300
}
deployment_spec {
  shared_object {
    name: "libc"
    upid {
      asid: 306070887 pid: 3902477011 ts_ns: 11841725277501915136
    }
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
)pxl";

TEST_F(ProbeCompilerTest, parse_single_probe) {
  std::string query = absl::Substitute(kSingleProbeUpsertPxlTpl, "$0",
                                       "px.uint128('123e4567-e89b-12d3-a456-426655440000')");
  ASSERT_OK_AND_ASSIGN(auto probe_ir, CompileProbeScript(query));
  plannerpb::CompileMutationsResponse pb;
  EXPECT_OK(probe_ir->ToProto(&pb));
  ASSERT_EQ(pb.mutations_size(), 1);
  EXPECT_THAT(pb.mutations()[0].trace(), testing::proto::EqualsProto(kSingleProbeProgramOnUPIDPb));
}

TEST_F(ProbeCompilerTest, parse_process_spec_just_pod) {
  std::string query =
      absl::Substitute(kSingleProbeUpsertPxlTpl, "$0",
                       "pxtrace.PodProcess('pl/vizier-query-broker-85dc9bc4d-jzw4s')");
  ASSERT_OK_AND_ASSIGN(auto probe_ir, CompileProbeScript(query));
  plannerpb::CompileMutationsResponse pb;
  EXPECT_OK(probe_ir->ToProto(&pb));
  ASSERT_EQ(pb.mutations_size(), 1);
  // TODO(yzhao) update on protobuf changes.
  EXPECT_THAT(pb.mutations()[0].trace().deployment_spec(),
              testing::proto::EqualsProto(kPodProcessDeploymentSpecJustPod));
}

TEST_F(ProbeCompilerTest, parse_process_spec_pod_and_container) {
  std::string query = absl::Substitute(
      kSingleProbeUpsertPxlTpl, "$0",
      "pxtrace.PodProcess('pl/vizier-query-broker-85dc9bc4d-jzw4s', 'querybroker')");
  ASSERT_OK_AND_ASSIGN(auto probe_ir, CompileProbeScript(query));
  plannerpb::CompileMutationsResponse pb;
  EXPECT_OK(probe_ir->ToProto(&pb));
  ASSERT_EQ(pb.mutations_size(), 1);
  EXPECT_THAT(pb.mutations()[0].trace().deployment_spec(),
              testing::proto::EqualsProto(kPodProcessDeploymentSpecNoProcessName));
}

TEST_F(ProbeCompilerTest, parse_process_spec) {
  std::string query =
      absl::Substitute(kSingleProbeUpsertPxlTpl, "$0",
                       "pxtrace.PodProcess('pl/vizier-query-broker-85dc9bc4d-jzw4s', "
                       "'querybroker', '/app/querybroker')");
  ASSERT_OK_AND_ASSIGN(auto probe_ir, CompileProbeScript(query));
  plannerpb::CompileMutationsResponse pb;
  EXPECT_OK(probe_ir->ToProto(&pb));
  ASSERT_EQ(pb.mutations_size(), 1);
  EXPECT_THAT(pb.mutations()[0].trace().deployment_spec(),
              testing::proto::EqualsProto(kPodProcessDeploymentSpec));
}

TEST_F(ProbeCompilerTest, parse_single_probe_on_shared_object) {
  std::string query = absl::Substitute(
      kSingleProbeUpsertPxlTpl, "$0",
      "pxtrace.SharedObject('libc', px.uint128('123e4567-e89b-12d3-a456-426655440000'))");
  ASSERT_OK_AND_ASSIGN(auto probe_ir, CompileProbeScript(query));

  plannerpb::CompileMutationsResponse pb;
  EXPECT_OK(probe_ir->ToProto(&pb));
  ASSERT_EQ(pb.mutations_size(), 1);
  EXPECT_THAT(pb.mutations()[0].trace(),
              testing::proto::EqualsProto(kSingleProbeUpsertSharedObjectProgramPb));
}

TEST_F(ProbeCompilerTest, parse_label_selector_spec_default_ns) {
  std::string query = absl::Substitute(
      kUProbeBPFTraceUpsertPxlTpl,
      "pxtrace.LabelSelector({'app': 'querybroker'},"
      "namespace='default', container_name='querybroker', process_name='/app/querybroker')");
  ASSERT_OK_AND_ASSIGN(auto probe_ir, CompileProbeScript(query));

  plannerpb::CompileMutationsResponse pb;
  EXPECT_OK(probe_ir->ToProto(&pb));
  ASSERT_EQ(pb.mutations_size(), 1);
  EXPECT_THAT(pb.mutations()[0].trace().deployment_spec(),
              testing::proto::EqualsProto(klabelSelectorDeploymentSpecDefaultNS));
}

TEST_F(ProbeCompilerTest, parse_label_selector_spec) {
  std::string query = absl::Substitute(kUProbeBPFTraceUpsertPxlTpl,
                                       "pxtrace.LabelSelector({'app': 'querybroker'},"
                                       "'pl', 'querybroker', '/app/querybroker')");
  ASSERT_OK_AND_ASSIGN(auto probe_ir, CompileProbeScript(query));

  plannerpb::CompileMutationsResponse pb;
  EXPECT_OK(probe_ir->ToProto(&pb));
  ASSERT_EQ(pb.mutations_size(), 1);
  EXPECT_THAT(pb.mutations()[0].trace().deployment_spec(),
              testing::proto::EqualsProto(kLabelSelectorDeploymentSpec));
}

TEST_F(ProbeCompilerTest, parse_single_probe_in_func) {
  FuncToExecute func_to_execute;
  func_to_execute.set_func_name("probe_table");
  func_to_execute.set_output_table_prefix("output");
  auto duration = func_to_execute.add_arg_values();
  duration->set_name("upid");
  duration->set_value("123e4567-e89b-12d3-a456-426655440000");

  ExecFuncs exec_funcs{func_to_execute};

  ASSERT_OK_AND_ASSIGN(auto probe_ir, CompileProbeScript(kSingleProbeInFuncPxl, exec_funcs));
  plannerpb::CompileMutationsResponse pb;
  EXPECT_OK(probe_ir->ToProto(&pb));
  ASSERT_EQ(pb.mutations_size(), 1);
  EXPECT_THAT(pb.mutations()[0].trace(), testing::proto::EqualsProto(kSingleProbeProgramOnUPIDPb));
}

constexpr char kHTTPBodyTracepointPb[] = R"pxl(
name: "http_body"
ttl {
  seconds: 300
}
deployment_spec {
  upid {
    asid: 1985274657 pid: 3902477011 ts_ns: 11841725277501915136
  }
}
programs {
  table_name: "http_body_table"
  spec {
    outputs {
      name: "http_body_table"
      fields: "req_body"
      fields: "resp_body"
    }
    probe {
      name: "http_body"
      tracepoint {
        symbol: "HTTPFunc"
      }
      args {
        id: "arg0"
        expr: "req_body"
      }
      args {
        id: "arg1"
        expr: "resp_body"
      }
      output_actions {
        output_name: "http_body_table"
        variable_names: "arg0"
        variable_names: "arg1"
      }
    }
  }
}
)pxl";

constexpr char kMultipleUpsertsInOneScriptTpl[] = R"pxl(
import pxtrace
import px

@pxtrace.probe("MyFunc")
def probe_func():
    id = pxtrace.ArgExpr('id')
    return [{'id': id},
            {'err': pxtrace.RetExpr('$$0.a')},
            {'latency': pxtrace.FunctionLatency()}]


pxtrace.UpsertTracepoint('http_return',
                         'http_return_table',
                         probe_func,
                         $0,
                         "5m")


@pxtrace.probe("HTTPFunc")
def http_body_probe():
    return [{'req_body': pxtrace.ArgExpr('req_body')},
            {'resp_body': pxtrace.ArgExpr('resp_body')}]


pxtrace.UpsertTracepoint('http_body',
                         'http_body_table',
                         http_body_probe,
                         $1,
                         "5m")

)pxl";

inline std::string WrapTraceMessage(std::string_view program) {
  return absl::Substitute("trace { $0 } ", program);
}

TEST_F(ProbeCompilerTest, probes_with_multiple_binaries) {
  // Test to make sure a probe definition doesn't add a probe.
  ASSERT_OK_AND_ASSIGN(auto probe_ir, CompileProbeScript(absl::Substitute(
                                          kMultipleUpsertsInOneScriptTpl,
                                          "px.uint128('123e4567-e89b-12d3-a456-426655440000')",
                                          "px.uint128('7654e321-e89b-12d3-a456-426655440000')")));
  plannerpb::CompileMutationsResponse pb;
  ASSERT_OK(probe_ir->ToProto(&pb));
  auto returnPb = pb.mutations()[0].trace();
  auto bodyPb = pb.mutations()[1].trace();
  if (returnPb.name() == "http_body") {
    auto tmp = bodyPb;
    bodyPb = returnPb;
    returnPb = tmp;
  }

  EXPECT_THAT(returnPb, testing::proto::EqualsProto(kSingleProbeProgramOnUPIDPb));
  EXPECT_THAT(bodyPb, testing::proto::EqualsProto(kHTTPBodyTracepointPb));
}

TEST_F(ProbeCompilerTest, probes_with_same_binary_succeeds) {
  // Test that makes sure we fail if we UpsertTracepoint > 1 time w/ same UPID then we fail.
  // The plan is to add another API endpoint.
  auto probe_ir_or_s = CompileProbeScript(absl::Substitute(
      kMultipleUpsertsInOneScriptTpl, "px.uint128('123e4567-e89b-12d3-a456-426655440000')",
      "px.uint128('123e4567-e89b-12d3-a456-426655440000')"));
  ASSERT_OK(probe_ir_or_s);
}

constexpr char kProbeNoReturn[] = R"pxl(
import pxtrace
import px

@pxtrace.probe("MyFunc")
def no_return_value_probe():
    id = pxtrace.ArgExpr('id')

pxtrace.UpsertTracepoint('my_http_return_value',
                    'no_ret_val',
                    no_return_value_probe,
                    px.uint128("7654e321-e89b-12d3-a456-426655440000"),
                    "5m")
)pxl";

TEST_F(ProbeCompilerTest, parse_probe_no_return_value) {
  auto probe_ir_or_s = CompileProbeScript(kProbeNoReturn);
  ASSERT_NOT_OK(probe_ir_or_s);
  EXPECT_THAT(
      probe_ir_or_s.status(),
      HasCompilerError(
          "Improper probe definition: missing output spec of probe, add a return statement"));
}

constexpr char kProbeDefNoUpsertPxl[] = R"pxl(
import pxtrace
import px

@pxtrace.probe("MyFunc")
def probe_func():
    return [{'id': pxtrace.ArgExpr('id')},
            {'err': pxtrace.RetExpr('$0.a')},
            {'latency': pxtrace.FunctionLatency()}]
)pxl";

TEST_F(ProbeCompilerTest, probe_definition_no_upsert) {
  // Test to make sure a probe definition doesn't add a probe.
  ASSERT_OK_AND_ASSIGN(auto probe_ir, CompileProbeScript(kProbeDefNoUpsertPxl));
  plannerpb::CompileMutationsResponse pb;
  EXPECT_OK(probe_ir->ToProto(&pb));
  ASSERT_EQ(pb.mutations_size(), 0);
}

constexpr char kProbeTemplate[] = R"pxl(
import pxtrace
import px

$0

pxtrace.UpsertTracepoint('p1',
                    'tablename',
                    probe_func,
                    px.uint128("7654e321-e89b-12d3-a456-426655440000"),
                    "5m")
)pxl";

constexpr char kOldTableNameSpecification[] = R"pxl(
@pxtrace.probe("MyFunc")
def probe_func():
    return 'tablename', [{'id': pxtrace.ArgExpr('id')}]
)pxl";

constexpr char kNoCollectionReturnValue[] = R"pxl(
@pxtrace.probe("MyFunc")
def probe_func():
    return 'tablename'
)pxl";

constexpr char kBadOutputColumnFormat[] = R"pxl(
@pxtrace.probe("MyFunc")
def probe_func():
    # should be a dict not a tracing variable
    return [pxtrace.ArgExpr('id')]
)pxl";

constexpr char kBadOutputColumnKey[] = R"pxl(
@pxtrace.probe("MyFunc")
def probe_func():
    # key is invalid.
    return [{pxtrace.ArgExpr('id'): pxtrace.ArgExpr('id')}]
)pxl";

constexpr char kBadOutputColumnValue[] = R"pxl(
@pxtrace.probe("MyFunc")
def probe_func():
    # value should be a probe value.
    return [{"id": "id"}]
)pxl";

TEST_F(ProbeCompilerTest, probe_definition_wrong_return_values) {
  // Test to make sure a probe definition doesn't add a probe.
  auto probe_ir_or_s =
      CompileProbeScript(absl::Substitute(kProbeTemplate, kOldTableNameSpecification));
  ASSERT_NOT_OK(probe_ir_or_s);
  EXPECT_THAT(probe_ir_or_s.status(), HasCompilerError("Expected Dict, got String"));

  probe_ir_or_s = CompileProbeScript(absl::Substitute(kProbeTemplate, kNoCollectionReturnValue));
  ASSERT_NOT_OK(probe_ir_or_s);
  EXPECT_THAT(probe_ir_or_s.status(),
              HasCompilerError(
                  "Unable to parse probe output definition. Expected Collection, received String"));

  probe_ir_or_s = CompileProbeScript(absl::Substitute(kProbeTemplate, kBadOutputColumnFormat));
  ASSERT_NOT_OK(probe_ir_or_s);
  EXPECT_THAT(probe_ir_or_s.status(), HasCompilerError("Expected Dict, got TracingVariable"));

  probe_ir_or_s = CompileProbeScript(absl::Substitute(kProbeTemplate, kBadOutputColumnKey));
  ASSERT_NOT_OK(probe_ir_or_s);
  EXPECT_THAT(probe_ir_or_s.status(),
              HasCompilerError("Expected 'String' in arg 'key', got 'tracingvariable'"));

  probe_ir_or_s = CompileProbeScript(absl::Substitute(kProbeTemplate, kBadOutputColumnValue));
  ASSERT_NOT_OK(probe_ir_or_s);
  EXPECT_THAT(probe_ir_or_s.status(), HasCompilerError("Expected TracingVariable, got String"));
}

TEST_F(ProbeCompilerTest, delete_tracepoint) {
  ASSERT_OK_AND_ASSIGN(
      auto probe_ir, CompileProbeScript("import pxtrace\npxtrace.DeleteTracepoint('http_return')"));
  EXPECT_THAT(probe_ir->TracepointsToDelete(), UnorderedElementsAre("http_return"));

  ASSERT_OK_AND_ASSIGN(probe_ir,
                       CompileProbeScript("import "
                                          "pxtrace\npxtrace.DeleteTracepoint('http_return')"
                                          "\npxtrace.DeleteTracepoint('cool_http_func')"));
  EXPECT_THAT(probe_ir->TracepointsToDelete(),
              UnorderedElementsAre("http_return", "cool_http_func"));
}

constexpr char kBPFTraceProgram[] = R"bpftrace(
tracepoint:syscalls:sys_enter_write
{
  @fds[tid] = args->fd
}

tracepoint:syscalls:sys_exit_write
{
  printf("tgid: %d ktime_ns: %d fd: %d ret: %d\\n",
         pid, nsecs, @fds[tid], args->ret);
}
)bpftrace";

constexpr char kBPFTracePxl[] = R"pxl(
import pxtrace
import px

bpftrace_syscall_write_program = """$0"""

pxtrace.UpsertTracepoint('syscall_write_bpftrace',
                         'output_table',
                         bpftrace_syscall_write_program,
                         pxtrace.kprobe(),
                         '5m')
)pxl";

constexpr char kBPFTraceProgramPb[] = R"proto(
name: "syscall_write_bpftrace"
ttl {
  seconds: 300
}
programs{
  table_name: "output_table"
  bpftrace {
    program: "$0"
  }
}
)proto";

TEST_F(ProbeCompilerTest, parse_bpftrace) {
  ASSERT_OK_AND_ASSIGN(auto probe_ir,
                       CompileProbeScript(absl::Substitute(kBPFTracePxl, kBPFTraceProgram)));
  plannerpb::CompileMutationsResponse pb;
  EXPECT_OK(probe_ir->ToProto(&pb));
  ASSERT_EQ(pb.mutations_size(), 1);

  std::string literal_bpf_trace = kBPFTraceProgram;
  literal_bpf_trace = std::regex_replace(literal_bpf_trace, std::regex(R"(\\\n)"), R"(\\\\n)");
  literal_bpf_trace = std::regex_replace(literal_bpf_trace, std::regex("\n"), "\\n");
  literal_bpf_trace = std::regex_replace(literal_bpf_trace, std::regex("\""), "\\\"");

  EXPECT_THAT(pb.mutations()[0].trace(),
              testing::proto::EqualsProto(absl::Substitute(kBPFTraceProgramPb, literal_bpf_trace)));
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

// Test that we can compile/parse a single TraceProgram object with a valid selector
constexpr char kBPFSingleTraceProgramObjectPxl[] = R"pxl(
import pxtrace
import px

after_519_trace_program = pxtrace.TraceProgram(
  program="""$0""",
  min_kernel='5.19',
)

table_name = 'tcp_drop_table'
pxtrace.UpsertTracepoint('tcp_drop_tracer',
                          table_name,
                          after_519_trace_program,
                          pxtrace.kprobe(),
                          '10m')
)pxl";

constexpr char kBPFSingleTraceProgramObjectPb[] = R"proto(
name: "tcp_drop_tracer"
ttl {
  seconds: 600
}
programs {
  table_name: "tcp_drop_table"
  bpftrace {
    program: "$0"
  }
  selectors {
    selector_type: MIN_KERNEL
    value: "5.19"
  }
}
)proto";

TEST_F(ProbeCompilerTest, parse_single_bpftrace_program_object) {
  ASSERT_OK_AND_ASSIGN(auto probe_ir,
                       CompileProbeScript(absl::Substitute(kBPFSingleTraceProgramObjectPxl,
                                                           kBPFTraceProgramMinKernel)));
  plannerpb::CompileMutationsResponse pb;
  EXPECT_OK(probe_ir->ToProto(&pb));
  ASSERT_EQ(pb.mutations_size(), 1);

  std::string literal_bpf_trace_min = kBPFTraceProgramMinKernel;
  literal_bpf_trace_min = std::regex_replace(literal_bpf_trace_min, std::regex("\n"), "\\n");

  EXPECT_THAT(pb.mutations()[0].trace(),
              testing::proto::EqualsProto(
                  absl::Substitute(kBPFSingleTraceProgramObjectPb, literal_bpf_trace_min)));
}

// Test that we can compile a list of TraceProgram objects with valid selectors
constexpr char kBPFTraceProgramObjectsPxl[] = R"pxl(
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

constexpr char kBPFTraceProgramObjectsPb[] = R"proto(
name: "tcp_drop_tracer"
ttl {
  seconds: 600
}
programs {
  table_name: "tcp_drop_table"
  bpftrace {
    program: "$0"
  }
  selectors {
    selector_type: MAX_KERNEL
    value: "5.18"
  }
}
programs {
  table_name: "tcp_drop_table"
  bpftrace {
    program: "$1"
  }
  selectors {
    selector_type: MIN_KERNEL
    value: "5.19"
  }
}
)proto";

TEST_F(ProbeCompilerTest, parse_multiple_bpftrace_program_objects) {
  ASSERT_OK_AND_ASSIGN(auto probe_ir, CompileProbeScript(absl::Substitute(
                                          kBPFTraceProgramObjectsPxl, kBPFTraceProgramMinKernel,
                                          kBPFTraceProgramMaxKernel)));
  plannerpb::CompileMutationsResponse pb;
  EXPECT_OK(probe_ir->ToProto(&pb));
  ASSERT_EQ(pb.mutations_size(), 1);

  std::string literal_bpf_trace_min = kBPFTraceProgramMinKernel;
  literal_bpf_trace_min = std::regex_replace(literal_bpf_trace_min, std::regex("\n"), "\\n");

  std::string literal_bpf_trace_max = kBPFTraceProgramMaxKernel;
  literal_bpf_trace_max = std::regex_replace(literal_bpf_trace_max, std::regex("\n"), "\\n");

  EXPECT_THAT(pb.mutations()[0].trace(),
              testing::proto::EqualsProto(absl::Substitute(
                  kBPFTraceProgramObjectsPb, literal_bpf_trace_min, literal_bpf_trace_max)));
}

// Test that passing an unsupported selector type to TraceProgram throws a compiler error
constexpr char kBPFUnsupportedTraceProgramObjectSelectorPxl[] = R"pxl(
import pxtrace
import px

after_519_trace_program = pxtrace.TraceProgram(
  program="""$0""",
  min_kernel='5.19',
  my_unsupported_selector='12345',
)

table_name = 'tcp_drop_table'
pxtrace.UpsertTracepoint('tcp_drop_tracer',
                          table_name,
                          after_519_trace_program,
                          pxtrace.kprobe(),
                          '10m')
)pxl";

TEST_F(ProbeCompilerTest, parse_unsupported_selector_in_trace_program_object) {
  auto probe_ir_or_s = CompileProbeScript(kBPFUnsupportedTraceProgramObjectSelectorPxl);
  ASSERT_NOT_OK(probe_ir_or_s);
  EXPECT_THAT(
      probe_ir_or_s.status(),
      HasCompilerError("Unsupported selector argument provided \'my_unsupported_selector\'"));
}

// Test that an invalid selector value throws a compiler error (currently needs to be a string)
constexpr char kBPFInvalidTraceProgramObjectSelectorPxl[] = R"pxl(
import pxtrace
import px

after_519_trace_program = pxtrace.TraceProgram(
  program="""$0""",
  min_kernel='5.19',
  max_kernel=None,
)

table_name = 'tcp_drop_table'
pxtrace.UpsertTracepoint('tcp_drop_tracer',
                          table_name,
                          after_519_trace_program,
                          pxtrace.kprobe(),
                          '10m')
)pxl";

TEST_F(ProbeCompilerTest, parse_invalid_trace_program_object) {
  auto probe_ir_or_s = CompileProbeScript(kBPFInvalidTraceProgramObjectSelectorPxl);
  ASSERT_NOT_OK(probe_ir_or_s);
  EXPECT_THAT(probe_ir_or_s.status(),
              HasCompilerError("Expected \'String\' in arg \'max_kernel\', got \'none\'"));
}

// Test that an empty selector value throws a compiler error
constexpr char kBPFEmptyTraceProgramObjectSelectorPxl[] = R"pxl(
import pxtrace
import px

after_519_trace_program = pxtrace.TraceProgram(
  program="""$0""",
  min_kernel='5.19',
  max_kernel='',
)

table_name = 'tcp_drop_table'
pxtrace.UpsertTracepoint('tcp_drop_tracer',
                          table_name,
                          after_519_trace_program,
                          pxtrace.kprobe(),
                          '10m')
)pxl";

TEST_F(ProbeCompilerTest, parse_empty_trace_program_object) {
  auto probe_ir_or_s = CompileProbeScript(kBPFEmptyTraceProgramObjectSelectorPxl);
  ASSERT_NOT_OK(probe_ir_or_s);
  EXPECT_THAT(probe_ir_or_s.status(),
              HasCompilerError("Empty selector value provided for \'max_kernel\'"));
}

constexpr char kConfigChangePxl[] = R"pxl(
import pxconfig
import px

pxconfig.set_agent_config("pl/vizier-pem-8xd7f", "gprof", "true")

)pxl";
constexpr char kConfigMutationPb[] = R"proto(
config_update {
  key: "gprof"
  value: "true"
  agent_pod_name: "pl/vizier-pem-8xd7f"
}
)proto";

TEST_F(ProbeCompilerTest, config_update) {
  ASSERT_OK_AND_ASSIGN(auto probe_ir, CompileProbeScript(kConfigChangePxl));
  plannerpb::CompileMutationsResponse pb;
  EXPECT_OK(probe_ir->ToProto(&pb));
  ASSERT_EQ(pb.mutations_size(), 1);

  EXPECT_THAT(pb.mutations()[0], testing::proto::EqualsProto(kConfigMutationPb));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
