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

#pragma once

#include <utility>
#include <vector>

#include <cstdio>
#include <fstream>
#include <memory>
#include <regex>
#include <string>
#include <unordered_set>

#include <absl/container/flat_hash_set.h>
#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/ast_visitor.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/distributedpb/distributed_plan.pb.h"
#include "src/carnot/planner/metadata/metadata_handler.h"
#include "src/carnot/planner/parser/parser.h"
#include "src/carnot/planner/parser/string_reader.h"
#include "src/carnot/planner/probes/tracing_module.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/typespb/types.pb.h"

namespace px {
namespace carnot {
namespace planner {

#define _MATCH_FAIL_IMPL(value, matcher) \
  absl::Substitute("Matcher '$1' didn't match '$0'", v->DebugString(), #matcher)

#define _NOT_MATCH_FAIL_IMPL(value, matcher) \
  absl::Substitute("Matcher '$1' unexpectedly matched '$0'", v->DebugString(), #matcher)

// Macros to better support match tests.
#define EXPECT_MATCH(value, matcher)                                    \
  do {                                                                  \
    auto v = value;                                                     \
    EXPECT_TRUE(Match(v, matcher)) << _MATCH_FAIL_IMPL(value, matcher); \
  } while (false)

#define ASSERT_MATCH(value, matcher)                                    \
  do {                                                                  \
    auto v = value;                                                     \
    ASSERT_TRUE(Match(v, matcher)) << _MATCH_FAIL_IMPL(value, matcher); \
  } while (false)

#define EXPECT_NOT_MATCH(value, matcher)                                     \
  do {                                                                       \
    auto v = value;                                                          \
    EXPECT_FALSE(Match(v, matcher)) << _NOT_MATCH_FAIL_IMPL(value, matcher); \
  } while (false)

#define ASSERT_NOT_MATCH(value, matcher)                                     \
  do {                                                                       \
    auto v = value;                                                          \
    ASSERT_FALSE(Match(v, matcher)) << _NOT_MATCH_FAIL_IMPL(value, matcher); \
  } while (false)

using table_store::schema::Relation;

constexpr char kExpectedUDFInfo[] = R"(
scalar_udfs {
  name: "divide"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type:FLOAT64
}
scalar_udfs {
  name: "contains"
  exec_arg_types: STRING
  exec_arg_types: STRING
  return_type:BOOLEAN
}
scalar_udfs {
  name: "equals"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type:BOOLEAN
}
scalar_udfs {
  name: "equals"
  exec_arg_types: FLOAT64
  exec_arg_types: INT64
  return_type:BOOLEAN
}
scalar_udfs {
  name: "divide"
  exec_arg_types: INT64
  exec_arg_types: FLOAT64
  return_type:FLOAT64
}
scalar_udfs {
  name: "add"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type:  FLOAT64
}
scalar_udfs {
  name: "add"
  exec_arg_types: INT64
  exec_arg_types: FLOAT64
  return_type:  FLOAT64
}
scalar_udfs {
  name: "add"
  exec_arg_types: FLOAT64
  exec_arg_types: INT64
  return_type:  FLOAT64
}
scalar_udfs {
  name: "add"
  exec_arg_types: INT64
  exec_arg_types: INT64
  return_type:  INT64
}
scalar_udfs {
  name: "add"
  exec_arg_types: TIME64NS
  exec_arg_types: INT64
  return_type:  INT64
}
scalar_udfs {
  name: "equal"
  exec_arg_types: STRING
  exec_arg_types: STRING
  return_type: BOOLEAN
}
scalar_udfs {
  name: "equal"
  exec_arg_types: UINT128
  exec_arg_types: UINT128
  return_type: BOOLEAN
}
scalar_udfs {
  name: "equal"
  exec_arg_types: INT64
  exec_arg_types: INT64
  return_type: BOOLEAN
}
scalar_udfs {
  name: "notEqual"
  exec_arg_types: STRING
  exec_arg_types: STRING
  return_type: BOOLEAN
}
scalar_udfs {
  name: "multiply"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type:  FLOAT64
}
scalar_udfs {
  name: "logicalAnd"
  exec_arg_types: BOOLEAN
  exec_arg_types: BOOLEAN
  return_type:  BOOLEAN
}
scalar_udfs {
  name: "subtract"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type:  FLOAT64
}
scalar_udfs {
  name: "upid_to_service_id"
  exec_arg_types: UINT128
  return_type: STRING
}
scalar_udfs {
  name: "upid_to_service_name"
  exec_arg_types: UINT128
  return_type: STRING
}
scalar_udfs {
  name: "service_id_to_service_name"
  exec_arg_types: STRING
  return_type: STRING
}
scalar_udfs {
  name: "bin"
  exec_arg_types: TIME64NS
  exec_arg_types: INT64
  return_type: TIME64NS
}
scalar_udfs {
  name: "bin"
  exec_arg_types: INT64
  exec_arg_types: INT64
  return_type: INT64
}
scalar_udfs {
  name: "pluck"
  exec_arg_types: FLOAT64
  exec_arg_types: STRING
  return_type: FLOAT64
}
udas {
  name: "count"
  update_arg_types: FLOAT64
  finalize_type:  INT64
}
udas {
  name: "count"
  update_arg_types: INT64
  finalize_type:  INT64
}
udas {
  name: "count"
  update_arg_types: BOOLEAN
  finalize_type:  INT64
}
udas {
  name: "sum"
  update_arg_types: INT64
  finalize_type:  INT64
}
udas {
  name: "count"
  update_arg_types: STRING
  finalize_type:  INT64
}
udas {
  name: "mean"
  update_arg_types: FLOAT64
  finalize_type:  FLOAT64
}
udas {
  name: "quantiles"
  update_arg_types: FLOAT64
  finalize_type:  FLOAT64
}
)";

constexpr char kOneAgentDistributedState[] = R"proto(
carnot_info {
  agent_id {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000001
  }
  query_broker_address: "agent"
  has_grpc_server: false
  has_data_store: true
  processes_data: true
  accepts_remote_sources: false
}
carnot_info {
  agent_id {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000002
  }
  query_broker_address: "kelvin"
  grpc_address: "1111"
  has_grpc_server: true
  has_data_store: false
  processes_data: true
  accepts_remote_sources: true
  ssl_targetname: "kelvin.pl.svc"
}
)proto";

distributedpb::DistributedState LoadDistributedStatePb(const std::string& physical_state_txt) {
  distributedpb::DistributedState physical_state_pb;
  CHECK(google::protobuf::TextFormat::MergeFromString(physical_state_txt, &physical_state_pb));
  return physical_state_pb;
}

/**
 * @brief Makes a test ast ptr that makes testing IRnode
 * Init calls w/o queries not error out.
 *
 * @return pypa::AstPtr
 */
pypa::AstPtr MakeTestAstPtr() {
  pypa::Ast ast_obj(pypa::AstType::Bool);
  ast_obj.line = 0;
  ast_obj.column = 0;
  return std::make_shared<pypa::Ast>(ast_obj);
}
/**
 * @brief Parses a query.
 *
 * @param query_str
 */
StatusOr<std::shared_ptr<IR>> ParseQuery(const std::string& query) {
  std::shared_ptr<IR> ir = std::make_shared<IR>();
  auto info = std::make_shared<RegistryInfo>();
  udfspb::UDFInfo info_pb;
  google::protobuf::TextFormat::MergeFromString(kExpectedUDFInfo, &info_pb);
  PX_RETURN_IF_ERROR(info->Init(info_pb));
  auto compiler_state = std::make_unique<CompilerState>(
      std::make_unique<RelationMap>(), /* sensitive_columns */ SensitiveColumnMap{}, info.get(),
      /* time_now */ types::Time64NSValue(0),
      /* max_output_rows_per_table */ 0, "result_addr", "result_ssl_targetname",
      /* redaction_options */ RedactionOptions{}, nullptr, nullptr, planner::DebugInfo{});
  compiler::ModuleHandler module_handler;
  compiler::MutationsIR dynamic_trace;
  PX_ASSIGN_OR_RETURN(auto ast_walker,
                      compiler::ASTVisitorImpl::Create(ir.get(), &dynamic_trace,
                                                       compiler_state.get(), &module_handler));

  pypa::AstModulePtr ast;
  pypa::SymbolTablePtr symbols;
  pypa::ParserOptions options;
  pypa::Lexer lexer(std::make_unique<StringReader>(query));

  if (VLOG_IS_ON(1)) {
    options.printerrors = true;
  } else {
    options.printerrors = false;
  }

  if (pypa::parse(lexer, ast, symbols, options)) {
    PX_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));
  } else {
    return error::InvalidArgument("Parsing was unsuccessful, likely because of broken argument.");
  }

  return ir;
}

struct CompilerErrorMatcher {
  explicit CompilerErrorMatcher(std::string expected_compiler_error, int expected_line,
                                int expected_column)
      : expected_compiler_error_(std::move(expected_compiler_error)),
        expected_line_(expected_line),
        expected_column_(expected_column) {}

  bool MatchAndExplain(const Status& status, ::testing::MatchResultListener* listener) const {
    if (status.ok()) {
      (*listener) << "Status is ok, no compiler error found.";
      return false;
    }
    if (status.msg().empty()) {
      (*listener) << "Status does not have a message.";
      return false;
    }
    if (!status.has_context()) {
      (*listener) << "Status does not have a context.";
      return false;
    }
    if (!status.context()->Is<compilerpb::CompilerErrorGroup>()) {
      (*listener) << "Status context is not a CompilerErrorGroup.";
      return false;
    }
    compilerpb::CompilerErrorGroup error_group;
    if (!status.context()->UnpackTo(&error_group)) {
      (*listener) << "Couldn't unpack the error to a compiler error group.";
      return false;
    }

    if (error_group.errors_size() == 0) {
      (*listener) << "No compile errors found.";
      return false;
    }

    bool regex_matched = false;
    int matched_line, matched_col;
    std::vector<std::string> error_messages;
    std::regex re(expected_compiler_error_);
    for (int64_t i = 0; i < error_group.errors_size(); i++) {
      auto error = error_group.errors(i).line_col_error();
      std::string msg = error.message();
      std::smatch match;
      if (std::regex_search(msg, match, re)) {
        regex_matched = true;
        matched_line = error.line();
        matched_col = error.column();
        if (expected_line_ != -1 && expected_line_ != matched_line) {
          // We have an error, but it's not at the expected line.
          continue;
        }
        if (expected_line_ != -1 && expected_column_ != -1 && expected_column_ != matched_col) {
          // We have an error, but it's not at the expected column.
          continue;
        }
        return true;
      }
      error_messages.push_back(msg);
    }
    if (!regex_matched) {
      (*listener) << absl::Substitute("Regex '$0' not matched in compiler errors: '$1'",
                                      expected_compiler_error_, absl::StrJoin(error_messages, ","));
    } else {
      (*listener) << absl::Substitute(
          "Regex matched in compiler errors, but the expected line/col"
          "values are different. Error at $0:$1",
          matched_line, matched_col);
    }
    return false;
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "regex matches message: " << expected_compiler_error_;
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "does not regex match message: " << expected_compiler_error_;
  }

  std::string expected_compiler_error_;
  int expected_line_;
  int expected_column_;
};

struct UnorderedRelationMatcher {
  explicit UnorderedRelationMatcher(const Relation& expected_relation)
      : expected_relation_(expected_relation) {}

  bool MatchAndExplain(const Relation& actual_relation,
                       ::testing::MatchResultListener* listener) const {
    if (expected_relation_.NumColumns() != actual_relation.NumColumns()) {
      (*listener) << absl::Substitute(
          "Expected relation has $0 columns and actual relation has $1 columns",
          expected_relation_.NumColumns(), actual_relation.NumColumns());
      return false;
    }

    for (const auto& expected_colname : expected_relation_.col_names()) {
      if (!actual_relation.HasColumn(expected_colname)) {
        (*listener) << absl::Substitute(
            "Expected relation has column '$0' which is missing in actual relation $1",
            expected_colname, actual_relation.DebugString());
        return false;
      }
      if (actual_relation.GetColumnType(expected_colname) !=
          expected_relation_.GetColumnType(expected_colname)) {
        (*listener) << absl::Substitute(
            "Mismatched type for column '$0', expected '$1' but received '$2'", expected_colname,
            expected_relation_.GetColumnType(expected_colname),
            actual_relation.GetColumnType(expected_colname));
        return false;
      }
    }
    return true;
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "unordered equals relation: " << expected_relation_.DebugString();
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "does not unordered equal relation: " << expected_relation_.DebugString();
  }

  const Relation expected_relation_;
};

template <typename... Args>
inline ::testing::PolymorphicMatcher<CompilerErrorMatcher> HasCompilerError(
    Args... substitute_args) {
  return ::testing::MakePolymorphicMatcher(
      CompilerErrorMatcher(std::move(absl::Substitute(substitute_args...)), -1, -1));
}

template <typename... Args>
inline ::testing::PolymorphicMatcher<CompilerErrorMatcher> HasCompilerErrorAt(
    int line, int col, Args... substitute_args) {
  return ::testing::MakePolymorphicMatcher(
      CompilerErrorMatcher(std::move(absl::Substitute(substitute_args...)), line, col));
}

// Checks whether two relations match, but agnostic to column order.
template <typename... Args>
inline ::testing::PolymorphicMatcher<UnorderedRelationMatcher> UnorderedRelationMatches(
    Args... args) {
  return ::testing::MakePolymorphicMatcher(UnorderedRelationMatcher(args...));
}

class OperatorTests : public ::testing::Test {
 protected:
  void SetUp() override {
    ast = MakeTestAstPtr();
    graph = std::make_shared<IR>();
    SetUpImpl();
  }

  virtual void SetUpImpl() {}

  MemorySourceIR* MakeMemSource() { return MakeMemSource("table"); }

  MemorySourceIR* MakeMemSource(const std::string& name,
                                const std::vector<std::string>& col_names = {}) {
    return graph->CreateNode<MemorySourceIR>(ast, name, col_names).ConsumeValueOrDie();
  }

  MemorySourceIR* MakeMemSource(const table_store::schema::Relation& relation) {
    return MakeMemSource("table", relation);
  }

  MemorySourceIR* MakeMemSource(const std::string& table_name,
                                const table_store::schema::Relation& relation) {
    return MakeMemSource(table_name, relation, relation.col_names());
  }

  MemorySourceIR* MakeMemSource(const std::string& table_name,
                                const table_store::schema::Relation& relation,
                                const std::vector<std::string>& col_names) {
    MemorySourceIR* mem_source = MakeMemSource(table_name, col_names);
    std::vector<int64_t> column_index_map;
    for (const auto& name : col_names) {
      column_index_map.push_back(relation.GetColumnIndex(name));
    }
    mem_source->SetColumnIndexMap(column_index_map);
    return mem_source;
  }

  MapIR* MakeMap(OperatorIR* parent, const ColExpressionVector& col_map,
                 bool keep_input_columns = false) {
    MapIR* map =
        graph->CreateNode<MapIR>(ast, parent, col_map, keep_input_columns).ConsumeValueOrDie();
    return map;
  }

  MemorySinkIR* MakeMemSink(OperatorIR* parent, std::string name,
                            const std::vector<std::string>& out_cols) {
    auto sink = graph->CreateNode<MemorySinkIR>(ast, parent, name, out_cols).ConsumeValueOrDie();
    return sink;
  }

  MemorySinkIR* MakeMemSink(OperatorIR* parent, std::string name) {
    return MakeMemSink(parent, name, {});
  }

  FilterIR* MakeFilter(OperatorIR* parent, ExpressionIR* filter_expr) {
    FilterIR* filter = graph->CreateNode<FilterIR>(ast, parent, filter_expr).ConsumeValueOrDie();
    return filter;
  }

  LimitIR* MakeLimit(OperatorIR* parent, int64_t limit_value) {
    LimitIR* limit = graph->CreateNode<LimitIR>(ast, parent, limit_value).ConsumeValueOrDie();
    return limit;
  }

  LimitIR* MakeLimit(OperatorIR* parent, int64_t limit_value, bool pem_only) {
    LimitIR* limit =
        graph->CreateNode<LimitIR>(ast, parent, limit_value, pem_only).ConsumeValueOrDie();
    return limit;
  }

  BlockingAggIR* MakeBlockingAgg(OperatorIR* parent, const std::vector<ColumnIR*>& columns,
                                 const ColExpressionVector& col_agg) {
    BlockingAggIR* agg =
        graph->CreateNode<BlockingAggIR>(ast, parent, columns, col_agg).ConsumeValueOrDie();
    return agg;
  }

  RollingIR* MakeRolling(OperatorIR* parent, ColumnIR* window_col, int64_t window_size) {
    RollingIR* rolling =
        graph->CreateNode<RollingIR>(ast, parent, window_col, window_size).ConsumeValueOrDie();
    return rolling;
  }

  ColumnIR* MakeColumn(const std::string& name, int64_t parent_op_idx) {
    ColumnIR* column = graph->CreateNode<ColumnIR>(ast, name, parent_op_idx).ConsumeValueOrDie();
    return column;
  }

  ColumnIR* MakeColumn(const std::string& name, int64_t parent_op_idx,
                       const types::DataType& type) {
    ColumnIR* column = MakeColumn(name, parent_op_idx);
    EXPECT_OK(column->SetResolvedType(ValueType::Create(type, types::ST_NONE)));
    return column;
  }

  ColumnIR* MakeColumn(const std::string& name, int64_t parent_op_idx,
                       const table_store::schema::Relation& relation) {
    ColumnIR* column = MakeColumn(name, parent_op_idx);
    auto type =
        ValueType::Create(relation.GetColumnType(name), relation.GetColumnSemanticType(name));
    EXPECT_OK(column->SetResolvedType(type));
    return column;
  }

  StringIR* MakeString(std::string val) {
    return graph->CreateNode<StringIR>(ast, val).ConsumeValueOrDie();
  }
  UInt128IR* MakeUInt128(std::string val) {
    return graph->CreateNode<UInt128IR>(ast, val).ConsumeValueOrDie();
  }

  IntIR* MakeInt(int64_t val) { return graph->CreateNode<IntIR>(ast, val).ConsumeValueOrDie(); }

  FloatIR* MakeFloat(double val) {
    return graph->CreateNode<FloatIR>(ast, val).ConsumeValueOrDie();
  }

  FuncIR* MakeAddFunc(ExpressionIR* left, ExpressionIR* right) {
    return graph
        ->CreateNode<FuncIR>(ast, FuncIR::op_map.find("+")->second,
                             std::vector<ExpressionIR*>({left, right}))
        .ConsumeValueOrDie();
  }
  FuncIR* MakeSubFunc(ExpressionIR* left, ExpressionIR* right) {
    return graph
        ->CreateNode<FuncIR>(ast, FuncIR::op_map.find("-")->second,
                             std::vector<ExpressionIR*>({left, right}))
        .ConsumeValueOrDie();
  }
  FuncIR* MakeMultFunc(ExpressionIR* left, ExpressionIR* right) {
    return graph
        ->CreateNode<FuncIR>(ast, FuncIR::op_map.find("*")->second,
                             std::vector<ExpressionIR*>({left, right}))
        .ConsumeValueOrDie();
  }

  FuncIR* MakeEqualsFunc(ExpressionIR* left, ExpressionIR* right) {
    return graph
        ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::eq, "==", "equal"},
                             std::vector<ExpressionIR*>({left, right}))
        .ConsumeValueOrDie();
  }

  FuncIR* MakeFunc(const std::string& name, const std::vector<ExpressionIR*>& args) {
    return graph->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", name}, args)
        .ConsumeValueOrDie();
  }

  FuncIR* MakeFunc(const std::string& name, const std::vector<ExpressionIR*>& args,
                   const types::DataType& output_type) {
    auto res = graph->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", name}, args)
                   .ConsumeValueOrDie();
    EXPECT_OK(res->SetResolvedType(ValueType::Create(output_type, types::ST_NONE)));
    return res;
  }

  FuncIR* MakeAndFunc(ExpressionIR* left, ExpressionIR* right) {
    return graph
        ->CreateNode<FuncIR>(ast, FuncIR::op_map.find("and")->second,
                             std::vector<ExpressionIR*>({left, right}))
        .ConsumeValueOrDie();
  }

  FuncIR* MakeOrFunc(ExpressionIR* left, ExpressionIR* right) {
    return graph
        ->CreateNode<FuncIR>(ast, FuncIR::op_map.find("or")->second,
                             std::vector<ExpressionIR*>({left, right}))
        .ConsumeValueOrDie();
  }

  MetadataIR* MakeMetadataIR(const std::string& name, int64_t parent_op_idx) {
    MetadataIR* metadata =
        graph->CreateNode<MetadataIR>(ast, name, parent_op_idx).ConsumeValueOrDie();
    return metadata;
  }

  FuncIR* MakeMeanFunc(std::string func_name, ExpressionIR* value) {
    return graph
        ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", func_name},
                             std::vector<ExpressionIR*>({value}))
        .ConsumeValueOrDie();
  }
  FuncIR* MakeMeanFunc(ExpressionIR* value) { return MakeMeanFunc("mean", value); }

  FuncIR* MakeMeanFuncWithFloatType(std::string func_name, ExpressionIR* value) {
    auto res = graph
                   ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", func_name},
                                        std::vector<ExpressionIR*>({value}))
                   .ConsumeValueOrDie();
    return res;
  }
  FuncIR* MakeMeanFuncWithFloatType(ExpressionIR* value) {
    return MakeMeanFuncWithFloatType("mean", value);
  }

  FuncIR* MakeMeanFunc() {
    return graph
        ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "mean"},
                             std::vector<ExpressionIR*>{})
        .ConsumeValueOrDie();
  }

  FuncIR* MakeCountFunc(ExpressionIR* value) {
    return graph
        ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "count"},
                             std::vector<ExpressionIR*>({value}))
        .ConsumeValueOrDie();
  }

  std::shared_ptr<IR> SwapGraphBeingBuilt(std::shared_ptr<IR> new_graph) {
    std::shared_ptr<IR> old_graph = graph;
    graph = new_graph;
    return old_graph;
  }

  GRPCSourceGroupIR* MakeGRPCSourceGroup(int64_t source_id, TypePtr type) {
    GRPCSourceGroupIR* grpc_src_group =
        graph->CreateNode<GRPCSourceGroupIR>(ast, source_id, type).ConsumeValueOrDie();
    return grpc_src_group;
  }

  GRPCSinkIR* MakeGRPCSink(OperatorIR* parent, int64_t source_id) {
    GRPCSinkIR* grpc_sink =
        graph->CreateNode<GRPCSinkIR>(ast, parent, source_id).ConsumeValueOrDie();
    return grpc_sink;
  }

  GRPCSinkIR* MakeGRPCSink(OperatorIR* parent, std::string name,
                           const std::vector<std::string>& out_cols) {
    GRPCSinkIR* grpc_sink =
        graph->CreateNode<GRPCSinkIR>(ast, parent, name, out_cols).ConsumeValueOrDie();
    return grpc_sink;
  }

  GRPCSourceIR* MakeGRPCSource(TypePtr type) {
    GRPCSourceIR* grpc_src_group = graph->CreateNode<GRPCSourceIR>(ast, type).ConsumeValueOrDie();
    return grpc_src_group;
  }

  UnionIR* MakeUnion(std::vector<OperatorIR*> parents) {
    UnionIR* union_node = graph->CreateNode<UnionIR>(ast, parents).ConsumeValueOrDie();
    return union_node;
  }

  JoinIR* MakeJoin(const std::vector<OperatorIR*>& parents, const std::string& join_type,
                   const Relation& left_relation, const Relation& right_relation,
                   const std::vector<std::string> left_on_col_names,
                   const std::vector<std::string>& right_on_col_names,
                   const std::vector<std::string>& suffix_strs = std::vector<std::string>{"", ""}) {
    std::vector<ColumnIR*> left_on_cols;
    std::vector<ColumnIR*> right_on_cols;
    for (const auto& left_name : left_on_col_names) {
      left_on_cols.push_back(MakeColumn(left_name, 0, left_relation));
    }
    for (const auto& right_name : right_on_col_names) {
      right_on_cols.push_back(MakeColumn(right_name, 1, right_relation));
    }
    return graph
        ->CreateNode<JoinIR>(ast, parents, join_type, left_on_cols, right_on_cols, suffix_strs)
        .ConsumeValueOrDie();
  }

  JoinIR* MakeJoin(const std::vector<OperatorIR*>& parents, const std::string& join_type,
                   const std::vector<ColumnIR*>& left_on_cols,
                   const std::vector<ColumnIR*>& right_on_cols,
                   const std::vector<std::string>& suffix_strs = std::vector<std::string>{}) {
    return graph
        ->CreateNode<JoinIR>(ast, parents, join_type, left_on_cols, right_on_cols, suffix_strs)
        .ConsumeValueOrDie();
  }

  // Use this if you need a relation but don't care about the contents.
  table_store::schema::Relation MakeRelation() {
    return table_store::schema::Relation(
        std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64,
                                      types::DataType::FLOAT64, types::DataType::FLOAT64}),
        std::vector<std::string>({"count", "cpu0", "cpu1", "cpu2"}));
  }
  // Same as MakeRelation, but has a time column.
  table_store::schema::Relation MakeTimeRelation() {
    return table_store::schema::Relation(
        std::vector<types::DataType>({types::DataType::TIME64NS, types::DataType::FLOAT64,
                                      types::DataType::FLOAT64, types::DataType::FLOAT64}),
        std::vector<std::string>({"time_", "cpu0", "cpu1", "cpu2"}));
  }

  TabletSourceGroupIR* MakeTabletSourceGroup(MemorySourceIR* mem_source,
                                             const std::vector<types::TabletID>& tablet_key_values,
                                             const std::string& tablet_key) {
    TabletSourceGroupIR* group =
        graph->CreateNode<TabletSourceGroupIR>(ast, mem_source, tablet_key_values, tablet_key)
            .ConsumeValueOrDie();
    return group;
  }

  GroupByIR* MakeGroupBy(OperatorIR* parent, const std::vector<ColumnIR*>& groups) {
    GroupByIR* groupby = graph->CreateNode<GroupByIR>(ast, parent, groups).ConsumeValueOrDie();
    return groupby;
  }

  UDTFSourceIR* MakeUDTFSource(
      const udfspb::UDTFSourceSpec& udtf_spec,
      const absl::flat_hash_map<std::string, ExpressionIR*>& arg_value_map) {
    return graph->CreateNode<UDTFSourceIR>(ast, udtf_spec.name(), arg_value_map, udtf_spec)
        .ConsumeValueOrDie();
  }

  UDTFSourceIR* MakeUDTFSource(const udfspb::UDTFSourceSpec& udtf_spec,
                               const std::vector<std::string>& arg_names,
                               const std::vector<ExpressionIR*>& arg_values) {
    CHECK_EQ(arg_names.size(), arg_values.size()) << "arg names and size must be the same.";
    absl::flat_hash_map<std::string, ExpressionIR*> arg_value_map;
    for (size_t i = 0; i < arg_names.size(); ++i) {
      arg_value_map[arg_names[i]] = arg_values[i];
    }
    return MakeUDTFSource(udtf_spec, arg_value_map);
  }

  DropIR* MakeDrop(OperatorIR* parent, const std::vector<std::string>& drop_cols) {
    DropIR* drop = graph->CreateNode<DropIR>(ast, parent, drop_cols).ConsumeValueOrDie();
    return drop;
  }

  TimeIR* MakeTime(int64_t t) { return graph->CreateNode<TimeIR>(ast, t).ConsumeValueOrDie(); }

  pypa::AstPtr ast;
  std::shared_ptr<IR> graph;
};

/**
 * @brief This rule removes all (non-Operator) IR nodes that are not connected to an Operator.
 *
 */
class CleanUpStrayIRNodesRule : public Rule {
 public:
  CleanUpStrayIRNodesRule()
      : Rule(nullptr, /*use_topo*/ true, /*reverse_topological_execution*/ false) {}

 protected:
  StatusOr<bool> Apply(IRNode* ir_node) override {
    auto ir_graph = ir_node->graph();
    auto node_id = ir_node->id();

    if (Match(ir_node, Operator()) || connected_nodes_.contains(ir_node)) {
      for (int64_t child_id : ir_graph->dag().DependenciesOf(node_id)) {
        connected_nodes_.insert(ir_graph->Get(child_id));
      }
      return false;
    }
    PX_RETURN_IF_ERROR(ir_graph->DeleteNode(node_id));
    return true;
  }

 private:
  // For each IRNode that stays in the graph, we keep track of its children as well so we
  // keep them around.
  absl::flat_hash_set<IRNode*> connected_nodes_;
};

class RulesTest : public OperatorTests {
 protected:
  void SetUpRegistryInfo() { info_ = udfexporter::ExportUDFInfo().ConsumeValueOrDie(); }

  void SetUpImpl() override {
    SetUpRegistryInfo();
    auto rel_map = std::make_unique<RelationMap>();
    cpu_relation = table_store::schema::Relation(
        std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64,
                                      types::DataType::FLOAT64, types::DataType::FLOAT64}),
        std::vector<std::string>({"count", "cpu0", "cpu1", "cpu2"}));
    semantic_rel =
        Relation({types::INT64, types::FLOAT64, types::STRING}, {"bytes", "cpu", "str_col"},
                 {types::ST_BYTES, types::ST_PERCENT, types::ST_NONE});
    rel_map->emplace("cpu", cpu_relation);
    rel_map->emplace("semantic_table", semantic_rel);

    compiler_state_ = std::make_unique<CompilerState>(
        std::move(rel_map), /* sensitive_columns */ SensitiveColumnMap{}, info_.get(),
        /* time_now */ time_now,
        /* max_output_rows_per_table */ 0, "result_addr", "result_ssl_targetname",
        /* redaction_options */ RedactionOptions{}, nullptr, nullptr, planner::DebugInfo{});
    md_handler = MetadataHandler::Create();
  }

  FilterIR* MakeFilter(OperatorIR* parent) {
    auto constant1 = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
    auto column = MakeColumn("column", 0);

    auto filter_func = graph
                           ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::eq, "==", "equal"},
                                                std::vector<ExpressionIR*>{constant1, column})
                           .ValueOrDie();
    if (parent->is_type_resolved()) {
      EXPECT_OK(
          ResolveExpressionType(filter_func, compiler_state_.get(), {parent->resolved_type()}));
    }

    return graph->CreateNode<FilterIR>(ast, parent, filter_func).ValueOrDie();
  }

  FilterIR* MakeFilter(OperatorIR* parent, ColumnIR* filter_value) {
    auto constant1 = graph->CreateNode<StringIR>(ast, "value").ValueOrDie();
    FuncIR* filter_func =
        graph
            ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::eq, "==", "equal"},
                                 std::vector<ExpressionIR*>{constant1, filter_value})
            .ValueOrDie();
    return graph->CreateNode<FilterIR>(ast, parent, filter_func).ValueOrDie();
  }

  FilterIR* MakeFilter(OperatorIR* parent, FuncIR* filter_expr) {
    return graph->CreateNode<FilterIR>(ast, parent, filter_expr).ValueOrDie();
  }

  using OperatorTests::MakeBlockingAgg;
  BlockingAggIR* MakeBlockingAgg(OperatorIR* parent, ColumnIR* by_column, ColumnIR* fn_column) {
    auto agg_func = graph
                        ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "mean"},
                                             std::vector<ExpressionIR*>{fn_column})
                        .ValueOrDie();
    return graph
        ->CreateNode<BlockingAggIR>(ast, parent, std::vector<ColumnIR*>{by_column},
                                    ColExpressionVector{{"agg_fn", agg_func}})
        .ValueOrDie();
  }

  void TearDown() override {
    if (skip_check_stray_nodes_) {
      return;
    }
    CleanUpStrayIRNodesRule cleanup;
    auto before = graph->DebugString();
    auto result = cleanup.Execute(graph.get());
    ASSERT_OK(result);
    ASSERT_FALSE(result.ConsumeValueOrDie())
        << "Rule left stray non-Operator IRNodes in graph: " << before;
  }

  // skip_check_stray_nodes_ should only be set to 'true' for tests of rules when they return an
  // error.
  bool skip_check_stray_nodes_ = false;
  std::unique_ptr<CompilerState> compiler_state_;
  std::unique_ptr<RegistryInfo> info_;
  int64_t time_now = 1552607213931245000;
  table_store::schema::Relation cpu_relation;
  table_store::schema::Relation semantic_rel;
  std::unique_ptr<MetadataHandler> md_handler;
};

struct HasEdgeMatcher {
  HasEdgeMatcher(int64_t from, int64_t to) : from_(from), to_(to) {}

  bool MatchAndExplain(std::shared_ptr<IR> graph, ::testing::MatchResultListener* listener) const {
    return MatchAndExplain(graph.get(), listener);
  }
  bool MatchAndExplain(IR* graph, ::testing::MatchResultListener* listener) const {
    if (!graph->HasNode(from_)) {
      (*listener) << "graph doesn't have from node with id " << from_;
      return false;
    }
    if (!graph->HasNode(to_)) {
      (*listener) << "graph doesn't have to node with id " << to_;
      return false;
    }

    if (!graph->dag().HasEdge(from_, to_)) {
      (*listener) << absl::Substitute("no edge ($0, $1)", graph->Get(from_)->DebugString(),
                                      graph->Get(to_)->DebugString());
      return false;
    }

    (*listener) << absl::Substitute("has edge ($0, $1)", graph->Get(from_)->DebugString(),
                                    graph->Get(to_)->DebugString());
    return true;
  }

  void DescribeTo(::std::ostream* os) const {
    *os << absl::Substitute("has edge ($0, $1)", from_, to_);
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << absl::Substitute("does not have edge ($0, $1)", from_, to_);
  }

  int64_t from_;
  int64_t to_;
};

template <typename... Args>
inline ::testing::PolymorphicMatcher<HasEdgeMatcher> HasEdge(IRNode* from, IRNode* to) {
  return ::testing::MakePolymorphicMatcher(HasEdgeMatcher(from->id(), to->id()));
}

template <typename... Args>
inline ::testing::PolymorphicMatcher<HasEdgeMatcher> HasEdge(int64_t from, int64_t to) {
  return ::testing::MakePolymorphicMatcher(HasEdgeMatcher(from, to));
}

constexpr char kUDTFOpenNetworkConnections[] = R"proto(
name: "OpenNetworkConnections"
args {
  name: "upid"
  arg_type: UINT128
  semantic_type: ST_UPID
}
executor: UDTF_SUBSET_PEM
relation {
  columns {
    column_name: "time_"
    column_type: TIME64NS
    column_semantic_type: ST_NONE
  }
  columns {
    column_name: "fd"
    column_type: INT64
    column_semantic_type: ST_NONE
  }
  columns {
    column_name: "name"
    column_type: STRING
    column_semantic_type: ST_NONE
  }
}
)proto";

constexpr char kUDTFAgentUID[] = R"proto(
name: "RunningProcesses"
args {
  name: "agent_uid"
  arg_type: STRING
  semantic_type: ST_AGENT_UID
}
executor: UDTF_ALL_AGENTS
relation {
  columns {
    column_name: "asid"
    column_type: INT64
    column_semantic_type: ST_NONE
  }
  columns {
    column_name: "name"
    column_type: STRING
    column_semantic_type: ST_NONE
  }
}
)proto";

constexpr char kUDTFAllAgents[] = R"proto(
name: "_Test_MD_State"
executor: UDTF_ALL_AGENTS
relation {
  columns {
    column_name: "asid"
    column_type: INT64
    column_semantic_type: ST_NONE
  }
  columns {
    column_name: "debug_state"
    column_type: STRING
    column_semantic_type: ST_NONE
  }
}
)proto";

constexpr char kUDTFServiceUpTimePb[] = R"proto(
name: "ServiceUpTime"
executor: UDTF_ONE_KELVIN
relation {
  columns {
    column_name: "service"
    column_type: STRING
    column_semantic_type: ST_NONE
  }
  columns {
    column_name: "up_time"
    column_type: INT64
    column_semantic_type: ST_NONE
  }
}
)proto";

class ASTVisitorTest : public OperatorTests {
 protected:
  void SetUp() override {
    OperatorTests::SetUp();
    relation_map_ = std::make_unique<RelationMap>();

    registry_info_ = std::make_shared<RegistryInfo>();
    udfspb::UDFInfo info_pb = udfexporter::ExportUDFInfo().ConsumeValueOrDie()->info_pb();
    google::protobuf::TextFormat::MergeFromString(kExpectedUDFInfo, &info_pb);
    EXPECT_OK(registry_info_->Init(info_pb));
    // TODO(philkuz) remove this when we have a udtf registry.
    udfspb::UDTFSourceSpec spec;
    google::protobuf::TextFormat::MergeFromString(kUDTFOpenNetworkConnections, &spec);
    registry_info_->AddUDTF(spec);
    // TODO(philkuz) remove end.
    relation_map_ = std::make_unique<RelationMap>();
    cpu_relation.AddColumn(types::FLOAT64, "cpu0");
    cpu_relation.AddColumn(types::FLOAT64, "cpu1");
    cpu_relation.AddColumn(types::FLOAT64, "cpu2");
    cpu_relation.AddColumn(types::UINT128, "upid");
    cpu_relation.AddColumn(types::INT64, "agent_id");
    relation_map_->emplace("cpu", cpu_relation);

    table_store::schema::Relation non_float_relation;
    non_float_relation.AddColumn(types::INT64, "int_col");
    non_float_relation.AddColumn(types::FLOAT64, "float_col");
    non_float_relation.AddColumn(types::STRING, "string_col");
    non_float_relation.AddColumn(types::BOOLEAN, "bool_col");
    relation_map_->emplace("non_float_table", non_float_relation);

    network_relation.AddColumn(types::UINT128, "upid");
    network_relation.AddColumn(types::INT64, "bytes_in");
    network_relation.AddColumn(types::INT64, "bytes_out");
    network_relation.AddColumn(types::INT64, "agent_id");
    relation_map_->emplace("network", network_relation);

    Relation http_events_relation;
    http_events_relation.AddColumn(types::TIME64NS, "time_");
    http_events_relation.AddColumn(types::UINT128, "upid");
    http_events_relation.AddColumn(types::STRING, "remote_addr");
    http_events_relation.AddColumn(types::INT64, "remote_port");
    http_events_relation.AddColumn(types::INT64, "major_version");
    http_events_relation.AddColumn(types::INT64, "minor_version");
    http_events_relation.AddColumn(types::INT64, "content_type");
    http_events_relation.AddColumn(types::STRING, "req_headers");
    http_events_relation.AddColumn(types::STRING, "req_method");
    http_events_relation.AddColumn(types::STRING, "req_path");
    http_events_relation.AddColumn(types::STRING, "req_body");
    http_events_relation.AddColumn(types::STRING, "resp_headers");
    http_events_relation.AddColumn(types::INT64, "resp_status");
    http_events_relation.AddColumn(types::STRING, "resp_message");
    http_events_relation.AddColumn(types::STRING, "resp_body");
    http_events_relation.AddColumn(types::INT64, "resp_latency_ns");
    relation_map_->emplace("http_events", http_events_relation);

    auto max_output_rows_per_table = 10000;
    compiler_state_ = std::make_unique<CompilerState>(
        std::move(relation_map_), SensitiveColumnMap{}, registry_info_.get(), time_now,
        max_output_rows_per_table, "result_addr", "result_ssl_targetname", RedactionOptions{},
        nullptr, nullptr, DebugInfo{});
  }

  StatusOr<std::shared_ptr<IR>> CompileGraph(const std::string& query) {
    return CompileGraph(query, /* exec_funcs */ {}, /* module_name_to_pxl */ {});
  }

  /**
   * @brief ParseScript takes a script and an initial variable state then parses
   * and walks through the AST given this initial variable state, updating the state
   * with whatever was walked through.
   *
   * If you're testing Objects, create a var_table, fill it with the object(s) you want
   * to test, then write a script interacting with those objects and store the result
   * into a variable.
   *
   * Then you can check if the ParseScript actually succeeds and what data is stored
   * in the var table.
   */
  Status ParseScript(const std::shared_ptr<compiler::VarTable>& var_table,
                     const std::string& script) {
    Parser parser;
    PX_ASSIGN_OR_RETURN(pypa::AstModulePtr ast, parser.Parse(script));

    bool func_based_exec = false;
    absl::flat_hash_set<std::string> reserved_names;
    compiler::ModuleHandler module_handler;
    compiler::MutationsIR mutations_ir;
    PX_ASSIGN_OR_RETURN(auto ast_walker,
                        compiler::ASTVisitorImpl::Create(graph.get(), var_table, &mutations_ir,
                                                         compiler_state_.get(), &module_handler,
                                                         func_based_exec, reserved_names));

    return ast_walker->ProcessModuleNode(ast);
  }

  StatusOr<std::shared_ptr<IR>> CompileGraph(
      const std::string& query, const std::vector<plannerpb::FuncToExecute>& exec_funcs,
      const absl::flat_hash_map<std::string, std::string>& module_name_to_pxl = {}) {
    Parser parser;
    PX_ASSIGN_OR_RETURN(pypa::AstModulePtr ast, parser.Parse(query));

    std::shared_ptr<IR> ir = std::make_shared<IR>();
    bool func_based_exec = exec_funcs.size() > 0;
    absl::flat_hash_set<std::string> reserved_names;
    for (const auto& fn : exec_funcs) {
      reserved_names.insert(fn.output_table_prefix());
    }

    compiler::ModuleHandler module_handler;
    compiler::MutationsIR probe_ir;
    PX_ASSIGN_OR_RETURN(auto ast_walker,
                        compiler::ASTVisitorImpl::Create(ir.get(), &probe_ir, compiler_state_.get(),
                                                         &module_handler, func_based_exec,
                                                         reserved_names, module_name_to_pxl));

    PX_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));
    if (func_based_exec) {
      PX_RETURN_IF_ERROR(ast_walker->ProcessExecFuncs(exec_funcs));
    }
    return ir;
  }

  StatusOr<std::shared_ptr<compiler::ASTVisitorImpl>> CompileInspectAST(const std::string& query) {
    Parser parser;
    PX_ASSIGN_OR_RETURN(auto ast, parser.Parse(query));
    std::shared_ptr<IR> ir = std::make_shared<IR>();
    compiler::ModuleHandler module_handler;
    compiler::MutationsIR dynamic_trace;
    PX_ASSIGN_OR_RETURN(auto ast_walker,
                        compiler::ASTVisitorImpl::Create(ir.get(), &dynamic_trace,
                                                         compiler_state_.get(), &module_handler));
    PX_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));
    return ast_walker;
  }

  Status AddUDFToRegistry(std::string name, types::DataType return_type,
                          const std::vector<types::DataType>& init_arg_types,
                          const std::vector<types::DataType>& exec_arg_types) {
    udfspb::UDFInfo info_pb = registry_info_->info_pb();
    auto new_udf = info_pb.add_scalar_udfs();
    new_udf->set_name(name);
    new_udf->set_return_type(return_type);
    for (auto init_arg : init_arg_types) {
      new_udf->add_init_arg_types(init_arg);
    }
    for (auto exec_arg : exec_arg_types) {
      new_udf->add_exec_arg_types(exec_arg);
    }
    return registry_info_->Init(info_pb);
  }
  Status AddUDFToRegistry(std::string name, types::DataType return_type,
                          const std::vector<types::DataType>& exec_arg_types) {
    return AddUDFToRegistry(name, return_type, {}, exec_arg_types);
  }

  Status AddUDAToRegistry(std::string name, types::DataType finalize_type,
                          const std::vector<types::DataType>& init_arg_types,
                          const std::vector<types::DataType>& update_arg_types,
                          bool supports_partial) {
    udfspb::UDFInfo info_pb = registry_info_->info_pb();
    auto new_uda = info_pb.add_udas();
    new_uda->set_name(name);
    new_uda->set_finalize_type(finalize_type);
    new_uda->set_supports_partial(supports_partial);
    for (auto init_arg : init_arg_types) {
      new_uda->add_init_arg_types(init_arg);
    }
    for (auto update_arg : update_arg_types) {
      new_uda->add_update_arg_types(update_arg);
    }
    return registry_info_->Init(info_pb);
  }
  Status AddUDAToRegistry(std::string name, types::DataType finalize_type,
                          const std::vector<types::DataType>& update_arg_types,
                          bool supports_partial) {
    return AddUDAToRegistry(name, finalize_type, {}, update_arg_types, supports_partial);
  }
  Status AddUDAToRegistry(std::string name, types::DataType finalize_type,
                          const std::vector<types::DataType>& update_arg_types) {
    return AddUDAToRegistry(name, finalize_type, {}, update_arg_types, false);
  }

  Relation cpu_relation;
  Relation network_relation;
  std::shared_ptr<RegistryInfo> registry_info_;
  std::unique_ptr<RelationMap> relation_map_;
  std::unique_ptr<CompilerState> compiler_state_;
  int64_t time_now = 1552607213931245000;
};

void CompareClone(IRNode* new_ir, IRNode* old_ir, const std::string& err_string);

// Same as CompareClone, but do some extra checking about IDs and whether or not they are in the
// expected graph.
void CompareClone(IRNode* new_ir, IRNode* old_ir, bool should_share_graph,
                  const std::string& err_string) {
  auto do_share_graph = new_ir->graph() == old_ir->graph();
  EXPECT_EQ(should_share_graph, do_share_graph)
      << "Graph placement of nodes doesn't match expectations. " << err_string;
  if (!should_share_graph) {
    EXPECT_EQ(new_ir->id(), old_ir->id()) << "Expected IRs to share the same ID. " << err_string;
  }
  return CompareClone(new_ir, old_ir, err_string);
}

template <typename TNodeType>
void CompareCloneNode(TNodeType* new_ir, TNodeType* /* old_ir */, const std::string& err_string) {
  EXPECT_TRUE(false) << absl::Substitute("Unimplemented clone test function for node $0, $1",
                                         new_ir->DebugString(), err_string);
}

template <>
void CompareCloneNode(ColumnIR* new_ir, ColumnIR* old_ir, const std::string& err_string) {
  EXPECT_EQ(new_ir->col_name(), old_ir->col_name()) << err_string;
  auto new_referenced_op = new_ir->ReferencedOperator().ConsumeValueOrDie();
  auto old_referenced_op = old_ir->ReferencedOperator().ConsumeValueOrDie();
  auto new_containing_ops = new_ir->ContainingOperators().ConsumeValueOrDie();
  auto old_containing_ops = old_ir->ContainingOperators().ConsumeValueOrDie();

  if (new_ir->graph() != old_ir->graph()) {
    // Can't call CompareClone due to the circular dependency that would result.
    for (const auto& [i, new_containing_op] : Enumerate(new_containing_ops)) {
      auto old_containing_op = old_containing_ops[i];
      EXPECT_EQ(new_containing_op->id(), old_containing_op->id()) << err_string;
      EXPECT_NE(new_containing_op->graph(), old_containing_op->graph()) << absl::Substitute(
          "'$1' and '$2' should have container ops that are in different graphs. $0.", err_string,
          new_ir->DebugString(), old_ir->DebugString());
    }
  }
  EXPECT_EQ(new_containing_ops.size(), old_containing_ops.size());
  CompareClone(new_referenced_op, old_referenced_op, new_ir->graph() == old_ir->graph(),
               err_string);
}

template <>
void CompareCloneNode(MetadataIR* new_ir, MetadataIR* old_ir, const std::string& err_string) {
  CompareCloneNode<ColumnIR>(new_ir, old_ir, err_string);
  EXPECT_EQ(new_ir->property(), old_ir->property())
      << absl::Substitute("Expected Metadata properties to be the same. Got $1 vs $2. $0.",
                          err_string, new_ir->property()->name(), old_ir->property()->name());
  EXPECT_EQ(new_ir->name(), old_ir->name())
      << absl::Substitute("Expected Metadata names to be the same. Got $1 vs $2. $0.", err_string,
                          new_ir->name(), old_ir->name());
}

template <>
void CompareCloneNode(IntIR* new_ir, IntIR* old_ir, const std::string& err_string) {
  EXPECT_EQ(new_ir->val(), old_ir->val()) << err_string;
}

template <>
void CompareCloneNode(StringIR* new_ir, StringIR* old_ir, const std::string& err_string) {
  EXPECT_EQ(new_ir->str(), old_ir->str()) << err_string;
}

template <>
void CompareCloneNode(MapIR* new_ir, MapIR* old_ir, const std::string& err_string) {
  std::vector<ColumnExpression> new_col_exprs = new_ir->col_exprs();
  std::vector<ColumnExpression> old_col_exprs = old_ir->col_exprs();
  ASSERT_EQ(new_col_exprs.size(), old_col_exprs.size()) << err_string;
  for (size_t i = 0; i < new_col_exprs.size(); ++i) {
    ColumnExpression new_expr = new_col_exprs[i];
    ColumnExpression old_expr = old_col_exprs[i];
    EXPECT_EQ(new_expr.name, old_expr.name) << err_string;
    CompareClone(new_expr.node, old_expr.node, new_ir->graph() == old_ir->graph(), err_string);
  }
}

template <>
void CompareCloneNode(UnionIR* new_ir, UnionIR* old_ir, const std::string& err_string) {
  EXPECT_EQ(new_ir->column_mappings().size(), old_ir->column_mappings().size()) << err_string;

  for (size_t i = 0; i < new_ir->column_mappings().size(); ++i) {
    EXPECT_EQ(new_ir->column_mappings()[i].size(), old_ir->column_mappings()[i].size())
        << err_string;
    for (size_t j = 0; j < new_ir->column_mappings()[i].size(); ++j) {
      CompareClone(new_ir->column_mappings()[i][j], old_ir->column_mappings()[i][j],
                   new_ir->graph() == old_ir->graph(), err_string);
    }
  }
}

template <>
void CompareCloneNode(BlockingAggIR* new_ir, BlockingAggIR* old_ir, const std::string& err_string) {
  std::vector<ColumnExpression> new_col_exprs = new_ir->aggregate_expressions();
  std::vector<ColumnExpression> old_col_exprs = old_ir->aggregate_expressions();
  ASSERT_EQ(new_col_exprs.size(), old_col_exprs.size()) << err_string;
  for (size_t i = 0; i < new_col_exprs.size(); ++i) {
    ColumnExpression new_expr = new_col_exprs[i];
    ColumnExpression old_expr = old_col_exprs[i];
    EXPECT_EQ(new_expr.name, old_expr.name) << err_string;
    CompareClone(new_expr.node, old_expr.node, new_ir->graph() == old_ir->graph(), err_string);
  }

  std::vector<ColumnIR*> new_groups = new_ir->groups();
  std::vector<ColumnIR*> old_groups = old_ir->groups();
  ASSERT_EQ(new_groups.size(), old_groups.size()) << err_string;
  for (size_t i = 0; i < new_groups.size(); ++i) {
    CompareClone(new_groups[i], old_groups[i], new_ir->graph() == old_ir->graph(), err_string);
  }
}

template <>
void CompareCloneNode(GroupByIR* new_ir, GroupByIR* old_ir, const std::string& err_string) {
  std::vector<ColumnIR*> new_groups = new_ir->groups();
  std::vector<ColumnIR*> old_groups = old_ir->groups();
  ASSERT_EQ(new_groups.size(), old_groups.size()) << err_string;
  for (size_t i = 0; i < new_groups.size(); ++i) {
    CompareClone(new_groups[i], old_groups[i], new_ir->graph() == old_ir->graph(), err_string);
  }
}

template <>
void CompareCloneNode(MemorySourceIR* new_ir, MemorySourceIR* old_ir,
                      const std::string& err_string) {
  EXPECT_EQ(new_ir->table_name(), old_ir->table_name()) << err_string;
  EXPECT_EQ(new_ir->IsTimeStartSet(), old_ir->IsTimeStartSet()) << err_string;
  if (new_ir->IsTimeStartSet()) {
    EXPECT_EQ(new_ir->time_start_ns(), old_ir->time_start_ns()) << err_string;
  }
  EXPECT_EQ(new_ir->IsTimeStopSet(), old_ir->IsTimeStopSet()) << err_string;
  if (new_ir->IsTimeStopSet()) {
    EXPECT_EQ(new_ir->time_stop_ns(), old_ir->time_stop_ns()) << err_string;
  }
  EXPECT_EQ(new_ir->column_names(), old_ir->column_names()) << err_string;
  EXPECT_EQ(new_ir->column_index_map_set(), old_ir->column_index_map_set()) << err_string;
  EXPECT_EQ(new_ir->streaming(), old_ir->streaming()) << err_string;
}

template <>
void CompareCloneNode(MemorySinkIR* new_ir, MemorySinkIR* old_ir, const std::string& err_string) {
  EXPECT_EQ(new_ir->name(), old_ir->name()) << err_string;
  EXPECT_EQ(new_ir->out_columns(), old_ir->out_columns()) << err_string;
}

template <>
void CompareCloneNode(FilterIR* new_ir, FilterIR* old_ir, const std::string& err_string) {
  CompareClone(new_ir->filter_expr(), old_ir->filter_expr(), err_string);
}

template <>
void CompareCloneNode(LimitIR* new_ir, LimitIR* old_ir, const std::string& err_string) {
  EXPECT_EQ(new_ir->limit_value(), old_ir->limit_value()) << err_string;
  EXPECT_EQ(new_ir->limit_value_set(), old_ir->limit_value_set()) << err_string;
}

template <>
void CompareCloneNode(FuncIR* new_ir, FuncIR* old_ir, const std::string& err_string) {
  EXPECT_TRUE(new_ir->Equals(old_ir)) << err_string;
}

template <>
void CompareCloneNode(GRPCSourceGroupIR* new_ir, GRPCSourceGroupIR* old_ir,
                      const std::string& err_string) {
  EXPECT_EQ(new_ir->source_id(), old_ir->source_id()) << err_string;
  EXPECT_EQ(new_ir->grpc_address(), old_ir->grpc_address()) << err_string;
  EXPECT_EQ(new_ir->GRPCAddressSet(), old_ir->GRPCAddressSet()) << err_string;
}

template <>
void CompareCloneNode(GRPCSourceIR* /*new_ir*/, GRPCSourceIR* /*old_ir*/,
                      const std::string& /*err_string*/) {}

template <>
void CompareCloneNode(GRPCSinkIR* new_ir, GRPCSinkIR* old_ir, const std::string& err_string) {
  EXPECT_EQ(new_ir->DestinationAddressSet(), old_ir->DestinationAddressSet()) << err_string;
  EXPECT_EQ(new_ir->destination_address(), old_ir->destination_address()) << err_string;
  EXPECT_EQ(new_ir->destination_ssl_targetname(), old_ir->destination_ssl_targetname())
      << err_string;
  EXPECT_EQ(new_ir->destination_id(), old_ir->destination_id()) << err_string;
  EXPECT_EQ(new_ir->has_destination_id(), old_ir->has_destination_id()) << err_string;
  EXPECT_EQ(new_ir->has_output_table(), old_ir->has_output_table()) << err_string;
  EXPECT_EQ(new_ir->out_columns(), old_ir->out_columns()) << err_string;
  EXPECT_EQ(new_ir->name(), old_ir->name()) << err_string;
  EXPECT_EQ(new_ir->out_columns(), old_ir->out_columns()) << err_string;
}

template <>
void CompareCloneNode(JoinIR* new_ir, JoinIR* old_ir, const std::string& err_string) {
  ASSERT_EQ(new_ir->join_type(), old_ir->join_type());
  EXPECT_THAT(new_ir->column_names(), ::testing::ElementsAreArray(old_ir->column_names()))
      << err_string;
  auto output_columns_new = new_ir->output_columns();
  auto output_columns_old = old_ir->output_columns();
  ASSERT_EQ(output_columns_new.size(), output_columns_old.size()) << err_string;

  for (size_t i = 0; i < output_columns_new.size(); ++i) {
    CompareClone(output_columns_new[i], output_columns_old[i], new_ir->graph() == old_ir->graph(),
                 absl::Substitute("$0; in Join operator.", err_string));
  }

  EXPECT_THAT(new_ir->suffix_strs(), ::testing::ElementsAreArray(old_ir->suffix_strs()))
      << err_string;

  auto left_on_columns_new = new_ir->left_on_columns();
  auto left_on_columns_old = old_ir->left_on_columns();
  for (size_t i = 0; i < left_on_columns_new.size(); ++i) {
    CompareClone(left_on_columns_new[i], left_on_columns_old[i], new_ir->graph() == old_ir->graph(),
                 absl::Substitute("$0; in Join operator.", err_string));
  }

  auto right_on_columns_new = new_ir->right_on_columns();
  auto right_on_columns_old = old_ir->right_on_columns();
  for (size_t i = 0; i < right_on_columns_new.size(); ++i) {
    CompareClone(right_on_columns_new[i], right_on_columns_old[i],
                 new_ir->graph() == old_ir->graph(),
                 absl::Substitute("$0; in Join operator.", err_string));
  }
}

void CompareClone(IRNode* new_ir, IRNode* old_ir, const std::string& err_string) {
  ASSERT_NE(new_ir, nullptr);
  ASSERT_NE(old_ir, nullptr);

  if (new_ir->graph() != old_ir->graph()) {
    EXPECT_EQ(new_ir->id(), new_ir->id()) << err_string;
  }
  // For all base classes that are derived from IRNode, check their properties here.
  if (Match(new_ir, Expression())) {
    EXPECT_EQ(new_ir->type_string(), old_ir->type_string()) << err_string;
    auto new_expr = static_cast<ExpressionIR*>(new_ir);
    auto old_expr = static_cast<ExpressionIR*>(old_ir);
    EXPECT_EQ(new_expr->annotations(), old_expr->annotations()) << err_string;
  }
  if (Match(new_ir, Operator())) {
    auto new_op = static_cast<OperatorIR*>(new_ir);
    auto old_op = static_cast<OperatorIR*>(old_ir);
    // Check type status.
    EXPECT_EQ(new_op->is_type_resolved(), old_op->is_type_resolved());
    EXPECT_TRUE(new_op->resolved_type()->Equals(old_op->resolved_type()));

    // Check parents.
    ASSERT_EQ(new_op->parents().size(), old_op->parents().size());
    for (size_t parent_idx = 0; parent_idx < new_op->parents().size(); ++parent_idx) {
      CompareClone(new_op->parents()[parent_idx], old_op->parents()[parent_idx],
                   new_ir->graph() == old_ir->graph(), err_string);
    }
  }

  EXPECT_EQ(new_ir->type(), old_ir->type()) << err_string;

  switch (new_ir->type()) {
#undef PX_CARNOT_IR_NODE
#define PX_CARNOT_IR_NODE(NAME)                                                             \
  case IRNodeType::k##NAME:                                                                 \
    return CompareCloneNode(static_cast<NAME##IR*>(new_ir), static_cast<NAME##IR*>(old_ir), \
                            err_string);
#include "src/carnot/planner/ir/ir_nodes.inl"
#undef PX_CARNOT_IR_NODE

    case IRNodeType::kAny:
    case IRNodeType::number_of_types:
      EXPECT_TRUE(false) << absl::Substitute("No compare func exists in test for $0",
                                             new_ir->DebugString());
  }
}

#define EXPECT_TableHasColumnWithType(table_type, col_name, expected_basic_type) \
  ASSERT_TRUE(table_type->HasColumn(col_name));                                  \
  EXPECT_EQ(*expected_basic_type, *std::static_pointer_cast<ValueType>(          \
                                      table_type->GetColumnType(col_name).ConsumeValueOrDie()));

#define EXPECT_COMPILER_ERROR(status, ...) \
  EXPECT_THAT(StatusAdapter(status), HasCompilerError(__VA_ARGS__))

#define ASSERT_COMPILER_ERROR(status, ...) \
  ASSERT_THAT(StatusAdapter(status), HasCompilerError(__VA_ARGS__))

#define EXPECT_COMPILER_ERROR_AT(status, line, col, ...) \
  EXPECT_THAT(StatusAdapter(status), HasCompilerErrorAt(line, col, __VA_ARGS__))

#define ASSERT_COMPILER_ERROR_AT(status, line, col, ...) \
  ASSERT_THAT(StatusAdapter(status), HasCompilerErrorAt(line, col, __VA_ARGS__))

MATCHER_P2(IsTableType, data_types, names,
           absl::StrCat(negation ? "doesn't" : "does", " match a TableType with col names ",
                        absl::StrJoin(names, ", "), " and data types ",
                        absl::StrJoin(data_types, ", "))) {
  return arg.Equals(TableType::Create(Relation(data_types, names)));
}

MATCHER_P(IsTableType, rel, "") { return arg.Equals(TableType::Create(rel)); }

void PrintTo(const TableType& table, std::ostream* os) { *os << table.DebugString(); }

}  // namespace planner
}  // namespace carnot
}  // namespace px
