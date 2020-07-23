#include "src/carnot/planner/probes/probes.h"
#include "src/carnot/planner/compiler/ast_visitor.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {
using ::testing::ContainsRegex;

class ProbeCompilerTest : public ASTVisitorTest {
 protected:
  StatusOr<std::shared_ptr<compiler::DynamicTraceIR>> CompileProbeScript(std::string_view query) {
    Parser parser;
    PL_ASSIGN_OR_RETURN(auto ast, parser.Parse(query));

    std::shared_ptr<IR> ir = std::make_shared<IR>();
    std::shared_ptr<compiler::DynamicTraceIR> probe_ir =
        std::make_shared<compiler::DynamicTraceIR>();

    ModuleHandler module_handler;
    PL_ASSIGN_OR_RETURN(auto ast_walker, compiler::ASTVisitorImpl::Create(
                                             ir.get(), probe_ir.get(), compiler_state_.get(),
                                             &module_handler, false, {}, {}));

    PL_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));
    return probe_ir;
  }
};

constexpr char kSingleProbePxl[] = R"pxl(
import pxtrace
import px

@pxtrace.goprobe("MyFunc")
def probe_func():
    id = pxtrace.ArgExpr('id')
    return "http_return_table", [{'id': id},
            {'err': pxtrace.RetExpr('$0.a')},
            {'latency': pxtrace.FunctionLatency()}]

pxtrace.UpsertTrace('http_return',
                    probe_func,
                    px.uint128("123e4567-e89b-12d3-a456-426655440000"),
                    "5m")
)pxl";

constexpr char kSingleProbeProgramPb[] = R"pxl(
binary_spec {
  upid {
    asid: 306070887 pid: 3902477011 ts_ns: 11841725277501915136
  }
  language: GOLANG
}
outputs {
  name: "http_return_table"
  fields: "id"
  fields: "err"
  fields: "latency"
}
probes {
  name: "http_return0"
  trace_point {
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
  ttl_ns: 300000000000
}
)pxl";

TEST_F(ProbeCompilerTest, parse_single_probe) {
  ASSERT_OK_AND_ASSIGN(auto probe_ir, CompileProbeScript(kSingleProbePxl));
  stirling::dynamic_tracing::ir::logical::Program prog_pb;
  EXPECT_OK(probe_ir->ToProto(&prog_pb));
  EXPECT_THAT(prog_pb, testing::proto::EqualsProto(kSingleProbeProgramPb));
}

constexpr char kMultipleProbePxl[] = R"pxl(
import pxtrace
import px

@pxtrace.goprobe("MyFunc")
def cool_func_probe():
    return "cool_func_table", [{'id': pxtrace.ArgExpr('id')},
            {'err': pxtrace.RetExpr('$0.a')},
            {'latency': pxtrace.FunctionLatency()}]


@pxtrace.goprobe("HTTPFunc")
def http_func_probe():
    return "http_table", [{'req_body': pxtrace.ArgExpr('req_body')},
            {'resp_body': pxtrace.ArgExpr('req_status')}]


pxtrace.UpsertTrace('myfunc',
                    [cool_func_probe, http_func_probe],
                    px.uint128("123e4567-e89b-12d3-a456-426655440000"),
                    "5m")
)pxl";

constexpr char kMultipleProbeProgramPb[] = R"pxl(
binary_spec {
  upid {
    asid: 306070887 pid: 3902477011 ts_ns: 11841725277501915136
  }
  language: GOLANG
}
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
  trace_point {
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
  ttl_ns: 300000000000
}
probes {
  name: "myfunc1"
  trace_point {
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
  ttl_ns: 300000000000
}
)pxl";

TEST_F(ProbeCompilerTest, parse_multiple_probes) {
  ASSERT_OK_AND_ASSIGN(auto probe_ir, CompileProbeScript(kMultipleProbePxl));
  stirling::dynamic_tracing::ir::logical::Program prog_pb;
  EXPECT_OK(probe_ir->ToProto(&prog_pb));
  EXPECT_THAT(prog_pb, testing::proto::EqualsProto(kMultipleProbeProgramPb));
}

constexpr char kMultipleBinariesInOneScript[] = R"pxl(
import pxtrace
import px

@pxtrace.goprobe("MyFunc")
def cool_func_probe():
    return "cool_func_table", [{'id': pxtrace.ArgExpr('id')},
            {'err': pxtrace.RetExpr('$0.a')},
            {'latency': pxtrace.FunctionLatency()}]


@pxtrace.goprobe("HTTPFunc")
def http_func_probe():
    return "http_table", [{'req_body': pxtrace.ArgExpr('req_body')},
            {'resp_body': pxtrace.ArgExpr('req_status')}]


pxtrace.UpsertTrace('cool_func',
                    cool_func_probe,
                    px.uint128("123e4567-e89b-12d3-a456-426655440000"),
                    "5m")

pxtrace.UpsertTrace('http_return_value',
                    http_func_probe,
                    px.uint128("7654e321-e89b-12d3-a456-426655440000"),
                    "5m")
)pxl";

TEST_F(ProbeCompilerTest, probes_with_multiple_binaries_fails) {
  // Test to make sure a probe definition doesn't add a probe.
  auto probe_ir_or_s = CompileProbeScript(kMultipleBinariesInOneScript);
  ASSERT_NOT_OK(probe_ir_or_s);
  EXPECT_THAT(probe_ir_or_s.status(),
              HasCompilerError("Probes for multiple processes not supported. Separate out "
                               "into different scripts"));
}

constexpr char kProbeNoReturn[] = R"pxl(
import pxtrace
import px

@pxtrace.goprobe("MyFunc")
def no_return_value_probe():
    id = pxtrace.ArgExpr('id')

pxtrace.UpsertTrace('my_http_return_value',
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

@pxtrace.goprobe("MyFunc")
def probe_func():
    return "cool_func", [{'id': pxtrace.ArgExpr('id')},
            {'err': pxtrace.RetExpr('$0.a')},
            {'latency': pxtrace.FunctionLatency()}]
)pxl";

TEST_F(ProbeCompilerTest, probe_definition_no_upsert) {
  // Test to make sure a probe definition doesn't add a probe.
  ASSERT_OK_AND_ASSIGN(auto probe_ir, CompileProbeScript(kProbeDefNoUpsertPxl));
  stirling::dynamic_tracing::ir::logical::Program prog_pb;
  EXPECT_OK(probe_ir->ToProto(&prog_pb));
  // Should equal the empty proto -> nothing was specified.
  EXPECT_THAT(prog_pb, testing::proto::EqualsProto(""));
}

constexpr char kProbeTemplate[] = R"pxl(
import pxtrace
import px

$0

pxtrace.UpsertTrace('p1',
                    probe_func,
                    px.uint128("7654e321-e89b-12d3-a456-426655440000"),
                    "5m")
)pxl";

constexpr char kMissingOutputName[] = R"pxl(
@pxtrace.goprobe("MyFunc")
def probe_func():
    return [{'id': pxtrace.ArgExpr('id')}]
)pxl";

constexpr char kNoColumnSpecification[] = R"pxl(
@pxtrace.goprobe("MyFunc")
def probe_func():
    return 'tablename', 'where a table def should be'
)pxl";

constexpr char kNoCollectionReturnValue[] = R"pxl(
@pxtrace.goprobe("MyFunc")
def probe_func():
    return 'tablename'
)pxl";

constexpr char kBadOutputColumnFormat[] = R"pxl(
@pxtrace.goprobe("MyFunc")
def probe_func():
    # should be a dict
    return 'tablename', [pxtrace.ArgExpr('id')]
)pxl";

constexpr char kBadOutputColumnKey[] = R"pxl(
@pxtrace.goprobe("MyFunc")
def probe_func():
    # should be a dict
    return 'tablename', [{pxtrace.ArgExpr('id'): "id"}]
)pxl";

constexpr char kBadOutputColumnValue[] = R"pxl(
@pxtrace.goprobe("MyFunc")
def probe_func():
    # should be a dict
    return 'tablename', [{"id": "id"}]
)pxl";

TEST_F(ProbeCompilerTest, probe_definition_wrong_return_values) {
  // Test to make sure a probe definition doesn't add a probe.
  auto probe_ir_or_s = CompileProbeScript(absl::Substitute(kProbeTemplate, kMissingOutputName));
  ASSERT_NOT_OK(probe_ir_or_s);
  // TODO(philkuz) (PP-2043) add AST to objects that don't have nodes.
  // EXPECT_THAT(
  //     probe_ir_or_s.status(),
  //     HasCompilerError("Expected return value to be Collection of length 2, got 1 elements"));
  EXPECT_THAT(probe_ir_or_s.status().msg(),
              ContainsRegex("Expected return value to be Collection of length 2, got 1 elements"));

  probe_ir_or_s = CompileProbeScript(absl::Substitute(kProbeTemplate, kNoColumnSpecification));
  ASSERT_NOT_OK(probe_ir_or_s);
  EXPECT_THAT(probe_ir_or_s.status(),
              HasCompilerError(
                  "Unable to parse probe output definition. Expected Collection, received String"));

  probe_ir_or_s = CompileProbeScript(absl::Substitute(kProbeTemplate, kNoCollectionReturnValue));
  ASSERT_NOT_OK(probe_ir_or_s);
  EXPECT_THAT(
      probe_ir_or_s.status(),
      HasCompilerError("Unable to parse probe return value. Expected Collection, received String"));

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

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
