#include "src/carnot/planner/probes/probes.h"
#include "src/carnot/planner/compiler/ast_visitor.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {
using ::testing::ContainsRegex;
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
    PL_ASSIGN_OR_RETURN(auto ast, parser.Parse(query));

    std::shared_ptr<IR> ir = std::make_shared<IR>();
    std::shared_ptr<compiler::MutationsIR> probe_ir = std::make_shared<compiler::MutationsIR>();

    ModuleHandler module_handler;
    PL_ASSIGN_OR_RETURN(auto ast_walker, compiler::ASTVisitorImpl::Create(
                                             ir.get(), probe_ir.get(), compiler_state_.get(),
                                             &module_handler, func_based_exec, reserved_names, {}));

    PL_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));
    if (func_based_exec) {
      PL_RETURN_IF_ERROR(ast_walker->ProcessExecFuncs(exec_funcs));
    }
    return probe_ir;
  }
};

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

//                Change this test case to pxtrace.uprobe or whatever we decide once supported.
constexpr char kSingleProbeUpsertSharedObjectPxl[] = R"pxl(
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
                         pxtrace.SharedObject('libc', px.uint128('123e4567-e89b-12d3-a456-426655440000')),
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

constexpr char kSingleProbeProgramPb[] = R"pxl(
name: "http_return"
ttl {
  seconds: 300
}
deployment_spec {
  upid {
    asid: 306070887 pid: 3902477011 ts_ns: 11841725277501915136
  }
}
tracepoints {
  output_name: "http_return_table"
  program {
    outputs {
      name: "http_return_table"
      fields: "id"
      fields: "err"
      fields: "latency"
    }
    probes {
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
        variable_name: "arg0"
        variable_name: "ret0"
        variable_name: "lat0"
      }
    }
  }
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
tracepoints {
  output_name: "http_return_table"
  program {
    outputs {
      name: "http_return_table"
      fields: "id"
      fields: "err"
      fields: "latency"
    }
    probes {
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
        variable_name: "arg0"
        variable_name: "ret0"
        variable_name: "lat0"
      }
    }
  }
}
)pxl";

TEST_F(ProbeCompilerTest, parse_single_probe) {
  ASSERT_OK_AND_ASSIGN(auto probe_ir, CompileProbeScript(kSingleProbePxl));
  plannerpb::CompileMutationsResponse pb;
  EXPECT_OK(probe_ir->ToProto(&pb));
  ASSERT_EQ(pb.mutations_size(), 1);
  EXPECT_THAT(pb.mutations()[0].trace(), testing::proto::EqualsProto(kSingleProbeProgramPb));
}

TEST_F(ProbeCompilerTest, parse_single_probe_on_shared_object) {
  ASSERT_OK_AND_ASSIGN(auto probe_ir, CompileProbeScript(kSingleProbeUpsertSharedObjectPxl));
  plannerpb::CompileMutationsResponse pb;
  EXPECT_OK(probe_ir->ToProto(&pb));
  ASSERT_EQ(pb.mutations_size(), 1);
  EXPECT_THAT(pb.mutations()[0].trace(),
              testing::proto::EqualsProto(kSingleProbeUpsertSharedObjectProgramPb));
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
  EXPECT_THAT(pb.mutations()[0].trace(), testing::proto::EqualsProto(kSingleProbeProgramPb));
}

constexpr char kMultipleProbePxl[] = R"pxl(
import pxtrace
import px

@pxtrace.probe("MyFunc")
def cool_func_probe():
    return "cool_func_table", [{'id': pxtrace.ArgExpr('id')},
            {'err': pxtrace.RetExpr('$0.a')},
            {'latency': pxtrace.FunctionLatency()}]


@pxtrace.probe("HTTPFunc")
def http_func_probe():
    return "http_table", [{'req_body': pxtrace.ArgExpr('req_body')},
            {'resp_body': pxtrace.ArgExpr('req_status')}]

# NOTE: syntax not supported yet.
pxtrace.UpsertTracepoints('myfunc',
                    px.uint128("123e4567-e89b-12d3-a456-426655440000"),
                    "5m")
                    .AddTracepoint("cool_func_table", cool_func_probe)
                    .AddTracepoint("http_table", http_func_probe)
)pxl";

constexpr char kMultipleProbeProgramPb[] = R"pxl(
ttl {
  seconds: 300
}
deployment_spec {
  upid {
    asid: 306070887 pid: 3902477011 ts_ns: 11841725277501915136
  }
}
tracepoints {
  output_name: "cool_func_table"
  program {
    outputs {
      name: "cool_func_table"
      fields: "id"
      fields: "err"
      fields: "latency"
    }
    outputs {
      name: "http_table"
      fields: "req_body"
      fields: "resp_body"
    }
    probes {
      name: "myfunc0"
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
        output_name: "cool_func_table"
        variable_name: "arg0"
        variable_name: "ret0"
        variable_name: "lat0"
      }
    }
    probes {
      name: "myfunc1"
      tracepoint {
        symbol: "HTTPFunc"
      }
      args {
        id: "arg0"
        expr: "req_body"
      }
      args {
        id: "arg1"
        expr: "req_status"
      }
      output_actions {
        output_name: "http_table"
        variable_name: "arg0"
        variable_name: "arg1"
      }
    }
  }
}
)pxl";

// TODO(philkuz) need to support multiple probe programs.
TEST_F(ProbeCompilerTest, DISABLED_parse_multiple_probes) {
  ASSERT_OK_AND_ASSIGN(auto probe_ir, CompileProbeScript(kMultipleProbePxl));
  plannerpb::CompileMutationsResponse pb;
  EXPECT_OK(probe_ir->ToProto(&pb));
  ASSERT_EQ(pb.mutations_size(), 1);
  EXPECT_THAT(pb.mutations()[0].trace(), testing::proto::EqualsProto(kMultipleProbeProgramPb));
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
tracepoints{
  output_name: "http_body_table"
  program {
    outputs {
      name: "http_body_table"
      fields: "req_body"
      fields: "resp_body"
    }
    probes {
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
        variable_name: "arg0"
        variable_name: "arg1"
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
                         px.uint128("$0"),
                         "5m")


@pxtrace.probe("HTTPFunc")
def http_body_probe():
    return [{'req_body': pxtrace.ArgExpr('req_body')},
            {'resp_body': pxtrace.ArgExpr('resp_body')}]


pxtrace.UpsertTracepoint('http_body',
                         'http_body_table',
                         http_body_probe,
                         px.uint128("$1"),
                         "5m")

)pxl";

inline std::string WrapTraceMessage(std::string_view program) {
  return absl::Substitute("trace { $0 } ", program);
}

TEST_F(ProbeCompilerTest, probes_with_multiple_binaries) {
  // Test to make sure a probe definition doesn't add a probe.
  ASSERT_OK_AND_ASSIGN(
      auto probe_ir, CompileProbeScript(absl::Substitute(kMultipleUpsertsInOneScriptTpl,
                                                         "123e4567-e89b-12d3-a456-426655440000",
                                                         "7654e321-e89b-12d3-a456-426655440000")));
  plannerpb::CompileMutationsResponse pb;
  ASSERT_OK(probe_ir->ToProto(&pb));
  auto returnPb = pb.mutations()[0].trace();
  auto bodyPb = pb.mutations()[1].trace();
  if (returnPb.name() == "http_body") {
    auto tmp = bodyPb;
    bodyPb = returnPb;
    returnPb = tmp;
  }

  EXPECT_THAT(returnPb, testing::proto::EqualsProto(kSingleProbeProgramPb));
  EXPECT_THAT(bodyPb, testing::proto::EqualsProto(kHTTPBodyTracepointPb));
}

TEST_F(ProbeCompilerTest, probes_with_same_binary_fails) {
  // Test that makes sure we fail if we UpsertTracepoint > 1 time w/ same UPID then we fail.
  // The plan is to add another API endpoint.
  auto probe_ir_or_s = CompileProbeScript(absl::Substitute(kMultipleUpsertsInOneScriptTpl,
                                                           "123e4567-e89b-12d3-a456-426655440000",
                                                           "123e4567-e89b-12d3-a456-426655440000"));
  ASSERT_NOT_OK(probe_ir_or_s);
  EXPECT_THAT(probe_ir_or_s.status(),
              HasCompilerError(
                  "Cannot UpsertTracepoint on the same binary. Use UpsertTracepoints instead"));
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
  // TODO(philkuz) (PP-2043) add AST to objects that don't have nodes.
  // EXPECT_THAT(probe_ir_or_s.status(), HasCompilerError("Expected Dict, got TracingVariable"));
  EXPECT_THAT(probe_ir_or_s.status().msg(), ContainsRegex("Expected Dict, got TracingVariable"));

  probe_ir_or_s = CompileProbeScript(absl::Substitute(kProbeTemplate, kBadOutputColumnKey));
  ASSERT_NOT_OK(probe_ir_or_s);
  // TODO(philkuz) (PP-2043) add AST to objects that don't have nodes.
  // EXPECT_THAT(probe_ir_or_s.status(), HasCompilerError("Expected String, got Tracing Variable"));
  EXPECT_THAT(probe_ir_or_s.status().msg(), ContainsRegex("Could not get IRNode from arg 'key'"));

  probe_ir_or_s = CompileProbeScript(absl::Substitute(kProbeTemplate, kBadOutputColumnValue));
  ASSERT_NOT_OK(probe_ir_or_s);
  EXPECT_THAT(probe_ir_or_s.status(), HasCompilerError("Expected tracing variable, got String"));
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

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
