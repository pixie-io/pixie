#pragma once

#include <utility>
#include <vector>

#include <cstdio>
#include <fstream>
#include <memory>
#include <regex>
#include <string>
#include <unordered_set>

#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/ast_visitor.h"
#include "src/carnot/compiler/compiler_state/compiler_state.h"
#include "src/carnot/compiler/parser/parser.h"
#include "src/carnot/compiler/parser/string_reader.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace carnot {
namespace compiler {

using table_store::schema::Relation;

const char* kExpectedUDFInfo = R"(
scalar_udfs {
  name: "pl.divide"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type:FLOAT64
}
scalar_udfs {
  name: "pl.divide"
  exec_arg_types: INT64
  exec_arg_types: FLOAT64
  return_type:FLOAT64
}
scalar_udfs {
  name: "pl.add"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type:  FLOAT64
}
scalar_udfs {
  name: "pl.add"
  exec_arg_types: INT64
  exec_arg_types: INT64
  return_type:  INT64
}
scalar_udfs {
  name: "pl.add"
  exec_arg_types: TIME64NS
  exec_arg_types: INT64
  return_type:  INT64
}
scalar_udfs {
  name: "pl.equal"
  exec_arg_types: STRING
  exec_arg_types: STRING
  return_type: BOOLEAN
}
scalar_udfs {
  name: "pl.equal"
  exec_arg_types: UINT128
  exec_arg_types: UINT128
  return_type: BOOLEAN
}
scalar_udfs {
  name: "pl.equal"
  exec_arg_types: INT64
  exec_arg_types: INT64
  return_type: BOOLEAN
}
scalar_udfs {
  name: "pl.notEqual"
  exec_arg_types: STRING
  exec_arg_types: STRING
  return_type: BOOLEAN
}
scalar_udfs {
  name: "pl.multiply"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type:  FLOAT64
}
scalar_udfs {
  name: "pl.logicalAnd"
  exec_arg_types: BOOLEAN
  exec_arg_types: BOOLEAN
  return_type:  BOOLEAN
}
scalar_udfs {
  name: "pl.subtract"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type:  FLOAT64
}
scalar_udfs {
  name: "pl.upid_to_service_id"
  exec_arg_types: UINT128
  return_type: STRING
}
scalar_udfs {
  name: "pl.upid_to_service_name"
  exec_arg_types: UINT128
  return_type: STRING
}
scalar_udfs {
  name: "pl.service_id_to_service_name"
  exec_arg_types: STRING
  return_type: STRING
}
scalar_udfs {
  name: "pl.bin"
  exec_arg_types: TIME64NS
  exec_arg_types: INT64
  return_type: TIME64NS
}
scalar_udfs {
  name: "pl.bin"
  exec_arg_types: INT64
  exec_arg_types: INT64
  return_type: INT64
}
scalar_udfs {
  name: "pl.pluck"
  exec_arg_types: FLOAT64
  exec_arg_types: STRING
  return_type: FLOAT64
}
udas {
  name: "pl.count"
  update_arg_types: FLOAT64
  finalize_type:  INT64
}
udas {
  name: "pl.count"
  update_arg_types: INT64
  finalize_type:  INT64
}
udas {
  name: "pl.count"
  update_arg_types: BOOLEAN
  finalize_type:  INT64
}
udas {
  name: "pl.sum"
  update_arg_types: INT64
  finalize_type:  INT64
}
udas {
  name: "pl.count"
  update_arg_types: STRING
  finalize_type:  INT64
}
udas {
  name: "pl.mean"
  update_arg_types: FLOAT64
  finalize_type:  FLOAT64
}
udas {
  name: "pl.quantiles"
  update_arg_types: FLOAT64
  finalize_type:  FLOAT64
}
)";

/**
 * @brief Finds the specified type in the graph and returns the node.
 *
 *
 * @param ir_graph
 * @param type
 * @return StatusOr<IRNode*> IRNode of type, otherwise returns an error.
 */
StatusOr<IRNode*> FindNodeType(std::shared_ptr<IR> ir_graph, IRNodeType type,
                               int64_t instance = 0) {
  int found = 0;
  for (auto& i : ir_graph->dag().TopologicalSort()) {
    auto node = ir_graph->Get(i);
    if (node->type() == type) {
      if (found == instance) {
        return node;
      }
      found++;
    }
  }
  return error::NotFound("Couldn't find node of type $0 in ir_graph.",
                         kIRNodeStrings[static_cast<int64_t>(type)]);
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
  PL_RETURN_IF_ERROR(info->Init(info_pb));
  auto compiler_state =
      std::make_shared<CompilerState>(std::make_unique<RelationMap>(), info.get(), 0);
  PL_ASSIGN_OR_RETURN(auto ast_walker, ASTVisitorImpl::Create(ir.get(), compiler_state.get()));

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
    PL_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));
  } else {
    return error::InvalidArgument("Parsing was unsuccessful, likely because of broken argument.");
  }

  return ir;
}

struct CompilerErrorMatcher {
  explicit CompilerErrorMatcher(std::string expected_compiler_error)
      : expected_compiler_error_(std::move(expected_compiler_error)) {}

  bool MatchAndExplain(const Status& status, ::testing::MatchResultListener* listener) const {
    if (status.ok()) {
      (*listener) << "Status is ok, no compiler error found.";
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

    std::vector<std::string> error_messages;
    std::regex re(expected_compiler_error_);
    for (int64_t i = 0; i < error_group.errors_size(); i++) {
      auto error = error_group.errors(i).line_col_error();
      std::string msg = error.message();
      std::smatch match;
      if (std::regex_search(msg, match, re)) {
        return true;
      }
      error_messages.push_back(msg);
    }
    (*listener) << absl::Substitute("Regex '$0' not matched in compiler errors: '$1'",
                                    expected_compiler_error_, absl::StrJoin(error_messages, ","));
    return false;
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "equals message: " << expected_compiler_error_;
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "does not equal message: " << expected_compiler_error_;
  }

  std::string expected_compiler_error_;
};

template <typename... Args>
inline ::testing::PolymorphicMatcher<CompilerErrorMatcher> HasCompilerError(
    Args... substitute_args) {
  return ::testing::MakePolymorphicMatcher(
      CompilerErrorMatcher(std::move(absl::Substitute(substitute_args...))));
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

  MemorySourceIR* MakeMemSource(const std::string& name) {
    return graph->CreateNode<MemorySourceIR>(ast, name, std::vector<std::string>{})
        .ConsumeValueOrDie();
  }

  MemorySourceIR* MakeMemSource(const table_store::schema::Relation& relation) {
    return MakeMemSource("table", relation);
  }

  MemorySourceIR* MakeMemSource(const std::string& table_name,
                                const table_store::schema::Relation& relation) {
    MemorySourceIR* mem_source = MakeMemSource(table_name);
    EXPECT_OK(mem_source->SetRelation(relation));
    std::vector<int64_t> column_index_map;
    for (size_t i = 0; i < relation.NumColumns(); ++i) {
      column_index_map.push_back(i);
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

  BlockingAggIR* MakeBlockingAgg(OperatorIR* parent, const std::vector<ColumnIR*>& columns,
                                 const ColExpressionVector& col_agg) {
    BlockingAggIR* agg =
        graph->CreateNode<BlockingAggIR>(ast, parent, columns, col_agg).ConsumeValueOrDie();
    return agg;
  }

  ColumnIR* MakeColumn(const std::string& name, int64_t parent_op_idx) {
    ColumnIR* column = graph->CreateNode<ColumnIR>(ast, name, parent_op_idx).ConsumeValueOrDie();
    return column;
  }

  ColumnIR* MakeColumn(const std::string& name, int64_t parent_op_idx,
                       const table_store::schema::Relation& relation) {
    ColumnIR* column = MakeColumn(name, parent_op_idx);
    column->ResolveColumn(relation.GetColumnIndex(name), relation.GetColumnType(name));
    return column;
  }

  StringIR* MakeString(std::string val) {
    return graph->CreateNode<StringIR>(ast, val).ConsumeValueOrDie();
  }

  IntIR* MakeInt(int64_t val) { return graph->CreateNode<IntIR>(ast, val).ConsumeValueOrDie(); }

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
        ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::eq, "==", "equals"},
                             std::vector<ExpressionIR*>({left, right}))
        .ConsumeValueOrDie();
  }

  FuncIR* MakeFunc(const std::string& name, const std::vector<ExpressionIR*>& args) {
    return graph->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", name}, args)
        .ConsumeValueOrDie();
  }

  FuncIR* MakeAndFunc(ExpressionIR* left, ExpressionIR* right) {
    return graph
        ->CreateNode<FuncIR>(ast, FuncIR::op_map.find("and")->second,
                             std::vector<ExpressionIR*>({left, right}))
        .ConsumeValueOrDie();
  }

  MetadataIR* MakeMetadataIR(const std::string& name, int64_t parent_op_idx) {
    MetadataIR* metadata =
        graph->CreateNode<MetadataIR>(ast, name, parent_op_idx).ConsumeValueOrDie();
    return metadata;
  }

  MetadataLiteralIR* MakeMetadataLiteral(DataIR* data_ir) {
    MetadataLiteralIR* metadata_literal =
        graph->CreateNode<MetadataLiteralIR>(ast, data_ir).ConsumeValueOrDie();
    return metadata_literal;
  }

  FuncIR* MakeMeanFunc(ExpressionIR* value) {
    return graph
        ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "mean"},
                             std::vector<ExpressionIR*>({value}))
        .ConsumeValueOrDie();
  }
  FuncIR* MakeMeanFunc() {
    return graph
        ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "mean"},
                             std::vector<ExpressionIR*>{})
        .ConsumeValueOrDie();
  }

  std::shared_ptr<IR> SwapGraphBeingBuilt(std::shared_ptr<IR> new_graph) {
    std::shared_ptr<IR> old_graph = graph;
    graph = new_graph;
    return old_graph;
  }

  GRPCSourceGroupIR* MakeGRPCSourceGroup(int64_t source_id,
                                         const table_store::schema::Relation& relation) {
    GRPCSourceGroupIR* grpc_src_group =
        graph->CreateNode<GRPCSourceGroupIR>(ast, source_id, relation).ConsumeValueOrDie();
    return grpc_src_group;
  }

  GRPCSinkIR* MakeGRPCSink(OperatorIR* parent, int64_t source_id) {
    GRPCSinkIR* grpc_sink =
        graph->CreateNode<GRPCSinkIR>(ast, parent, source_id).ConsumeValueOrDie();
    return grpc_sink;
  }

  GRPCSourceIR* MakeGRPCSource(const table_store::schema::Relation& relation) {
    GRPCSourceIR* grpc_src_group =
        graph->CreateNode<GRPCSourceIR>(ast, relation).ConsumeValueOrDie();
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
                   const std::vector<std::string>& suffix_strs = std::vector<std::string>{}) {
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

  template <typename... Args>
  ListIR* MakeList(Args... args) {
    ListIR* list =
        graph->CreateNode<ListIR>(ast, std::vector<ExpressionIR*>{args...}).ConsumeValueOrDie();
    return list;
  }

  template <typename... Args>
  TupleIR* MakeTuple(Args... args) {
    TupleIR* tuple =
        graph->CreateNode<TupleIR>(ast, std::vector<ExpressionIR*>{args...}).ConsumeValueOrDie();
    return tuple;
  }

  pypa::AstPtr ast;
  std::shared_ptr<IR> graph;
};

struct HasEdgeMatcher {
  HasEdgeMatcher(IRNode* from, IRNode* to) : from_(from), to_(to) {}

  bool MatchAndExplain(std::shared_ptr<IR> graph, ::testing::MatchResultListener* listener) const {
    return MatchAndExplain(graph.get(), listener);
  }
  bool MatchAndExplain(IR* graph, ::testing::MatchResultListener* listener) const {
    if (from_ == nullptr) {
      (*listener) << "from node is null";
      return false;
    }
    if (to_ == nullptr) {
      (*listener) << "to node is null";
      return false;
    }

    if (!graph->HasNode(from_->id())) {
      (*listener) << "graph doesn't have from node with id " << from_->id();
      return false;
    }
    if (!graph->HasNode(to_->id())) {
      (*listener) << "graph doesn't have to node with id " << to_->id();
      return false;
    }

    if (!graph->dag().HasEdge(from_->id(), to_->id())) {
      (*listener) << absl::Substitute("no edge ($0, $1)", from_->DebugString(), to_->DebugString());
      return false;
    }

    (*listener) << absl::Substitute("has edge ($0, $1)", from_->DebugString(), to_->DebugString());
    return true;
  }

  void DescribeTo(::std::ostream* os) const {
    if (from_ == nullptr) {
      *os << absl::Substitute("WARNING from not found.");
      return;
    } else if (to_ == nullptr) {
      *os << absl::Substitute("WARNING to not found.");
      return;
    }
    *os << absl::Substitute("has edge ($0, $1)", from_->DebugString(), to_->DebugString());
  }

  void DescribeNegationTo(::std::ostream* os) const {
    if (from_ == nullptr) {
      *os << absl::Substitute("WARNING from not found.");
      return;
    } else if (to_ == nullptr) {
      *os << absl::Substitute("WARNING to not found.");
      return;
    }
    *os << absl::Substitute("does not have edge ($0, $1)", from_->DebugString(),
                            to_->DebugString());
  }

  IRNode* from_;
  IRNode* to_;
};

template <typename... Args>
inline ::testing::PolymorphicMatcher<HasEdgeMatcher> HasEdge(IRNode* from, IRNode* to) {
  return ::testing::MakePolymorphicMatcher(HasEdgeMatcher(from, to));
}

class ASTVisitorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();
    relation_map_ = std::make_unique<RelationMap>();

    registry_info_ = std::make_shared<RegistryInfo>();
    udfspb::UDFInfo info_pb;
    google::protobuf::TextFormat::MergeFromString(kExpectedUDFInfo, &info_pb);
    EXPECT_OK(registry_info_->Init(info_pb));
    table_store::schema::Relation cpu_relation;
    relation_map_ = std::make_unique<RelationMap>();
    cpu_relation.AddColumn(types::FLOAT64, "cpu0");
    cpu_relation.AddColumn(types::FLOAT64, "cpu1");
    cpu_relation.AddColumn(types::FLOAT64, "cpu2");
    cpu_relation.AddColumn(types::UINT128, MetadataProperty::kUniquePIDColumn);
    cpu_relation.AddColumn(types::INT64, "agent_id");
    relation_map_->emplace("cpu", cpu_relation);

    table_store::schema::Relation non_float_relation;
    non_float_relation.AddColumn(types::INT64, "int_col");
    non_float_relation.AddColumn(types::FLOAT64, "float_col");
    non_float_relation.AddColumn(types::STRING, "string_col");
    non_float_relation.AddColumn(types::BOOLEAN, "bool_col");
    relation_map_->emplace("non_float_table", non_float_relation);

    Relation network_relation;
    network_relation.AddColumn(types::UINT128, MetadataProperty::kUniquePIDColumn);
    network_relation.AddColumn(types::INT64, "bytes_in");
    network_relation.AddColumn(types::INT64, "bytes_out");
    network_relation.AddColumn(types::INT64, "agent_id");
    relation_map_->emplace("network", network_relation);

    Relation http_events_relation;
    http_events_relation.AddColumn(types::TIME64NS, "time_");
    http_events_relation.AddColumn(types::UINT128, MetadataProperty::kUniquePIDColumn);
    http_events_relation.AddColumn(types::STRING, "remote_addr");
    http_events_relation.AddColumn(types::INT64, "remote_port");
    http_events_relation.AddColumn(types::INT64, "http_major_version");
    http_events_relation.AddColumn(types::INT64, "http_minor_version");
    http_events_relation.AddColumn(types::INT64, "http_content_type");
    http_events_relation.AddColumn(types::STRING, "http_req_headers");
    http_events_relation.AddColumn(types::STRING, "http_req_method");
    http_events_relation.AddColumn(types::STRING, "http_req_path");
    http_events_relation.AddColumn(types::STRING, "http_req_body");
    http_events_relation.AddColumn(types::STRING, "http_resp_headers");
    http_events_relation.AddColumn(types::INT64, "http_resp_status");
    http_events_relation.AddColumn(types::STRING, "http_resp_message");
    http_events_relation.AddColumn(types::STRING, "http_resp_body");
    http_events_relation.AddColumn(types::INT64, "http_resp_latency_ns");
    relation_map_->emplace("http_events", http_events_relation);

    compiler_state_ =
        std::make_unique<CompilerState>(std::move(relation_map_), registry_info_.get(), time_now);
  }

  StatusOr<std::shared_ptr<IR>> CompileGraph(const std::string& query) {
    Parser parser;
    PL_ASSIGN_OR_RETURN(pypa::AstModulePtr ast, parser.Parse(query));
    std::shared_ptr<IR> ir = std::make_shared<IR>();
    PL_ASSIGN_OR_RETURN(auto ast_walker, ASTVisitorImpl::Create(ir.get(), compiler_state_.get()));

    PL_RETURN_IF_ERROR(ast_walker->ProcessModuleNode(ast));
    return ir;
  }

  // TODO(philkuz) remove this  -> we now have a function for this in the Relation class.
  bool RelationEquality(const table_store::schema::Relation& r1,
                        const table_store::schema::Relation& r2) {
    std::vector<std::string> r1_names;
    std::vector<std::string> r2_names;
    std::vector<types::DataType> r1_types;
    std::vector<types::DataType> r2_types;
    if (r1.NumColumns() >= r2.NumColumns()) {
      r1_names = r1.col_names();
      r1_types = r1.col_types();
      r2_names = r2.col_names();
      r2_types = r2.col_types();
    } else {
      r1_names = r2.col_names();
      r1_types = r2.col_types();
      r2_names = r1.col_names();
      r2_types = r1.col_types();
    }
    for (size_t i = 0; i < r1_names.size(); i++) {
      std::string col1 = r1_names[i];
      auto type1 = r1_types[i];
      auto r2_iter = std::find(r2_names.begin(), r2_names.end(), col1);
      // if we can't find name in the second relation, then
      if (r2_iter == r2_names.end()) {
        return false;
      }
      int64_t r2_idx = std::distance(r2_names.begin(), r2_iter);
      if (r2_types[r2_idx] != type1) {
        return false;
      }
    }
    return true;
  }

  std::shared_ptr<RegistryInfo> registry_info_;
  std::unique_ptr<RelationMap> relation_map_;
  std::unique_ptr<CompilerState> compiler_state_;
  int64_t time_now = 1552607213931245000;
};

void CompareClone(IRNode* new_ir, IRNode* old_ir, const std::string& err_string);

template <typename TNodeType>
void CompareCloneNode(TNodeType* new_ir, TNodeType* /* old_ir */, const std::string& err_string) {
  EXPECT_TRUE(false) << absl::Substitute("Unimplemented clone test function for node $0, $1",
                                         new_ir->DebugString(), err_string);
}

template <>
void CompareCloneNode(ColumnIR* new_ir, ColumnIR* old_ir, const std::string& err_string) {
  EXPECT_EQ(new_ir->col_name(), old_ir->col_name()) << err_string;
  auto new_containing_op = new_ir->ContainingOperator().ConsumeValueOrDie();
  auto old_containing_op = old_ir->ContainingOperator().ConsumeValueOrDie();
  auto new_referenced_op = new_ir->ReferencedOperator().ConsumeValueOrDie();
  auto old_referenced_op = old_ir->ReferencedOperator().ConsumeValueOrDie();
  if (new_ir->graph_ptr() != old_ir->graph_ptr()) {
    EXPECT_NE(new_containing_op->graph_ptr(), old_containing_op->graph_ptr()) << absl::Substitute(
        "'$1' and '$2' should have container ops that are in different graphs. $0.", err_string,
        new_ir->DebugString(), old_ir->DebugString());
    EXPECT_NE(new_referenced_op->graph_ptr(), old_referenced_op->graph_ptr()) << absl::Substitute(
        "'$1' and '$2' should have referenced ops that are in different graphs. $0.", err_string,
        new_ir->DebugString(), old_ir->DebugString());
    EXPECT_EQ(new_containing_op->id(), old_containing_op->id()) << err_string;
    EXPECT_EQ(new_referenced_op->id(), old_referenced_op->id()) << err_string;
  }
  CompareClone(new_referenced_op, old_referenced_op, err_string);
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
    CompareClone(new_expr.node, old_expr.node, err_string);
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
    CompareClone(new_expr.node, old_expr.node, err_string);
  }

  std::vector<ColumnIR*> new_groups = new_ir->groups();
  std::vector<ColumnIR*> old_groups = old_ir->groups();
  ASSERT_EQ(new_groups.size(), old_groups.size()) << err_string;
  for (size_t i = 0; i < new_groups.size(); ++i) {
    CompareClone(new_groups[i], old_groups[i], err_string);
  }
}

template <>
void CompareCloneNode(GroupByIR* new_ir, GroupByIR* old_ir, const std::string& err_string) {
  std::vector<ColumnIR*> new_groups = new_ir->groups();
  std::vector<ColumnIR*> old_groups = old_ir->groups();
  ASSERT_EQ(new_groups.size(), old_groups.size()) << err_string;
  for (size_t i = 0; i < new_groups.size(); ++i) {
    CompareClone(new_groups[i], old_groups[i], err_string);
  }
}

template <>
void CompareCloneNode(MetadataLiteralIR* new_ir, MetadataLiteralIR* old_ir,
                      const std::string& err_string) {
  EXPECT_EQ(new_ir->literal_type(), old_ir->literal_type()) << err_string;
  CompareClone(new_ir->literal(), old_ir->literal(), err_string);
}

template <>
void CompareCloneNode(MemorySourceIR* new_ir, MemorySourceIR* old_ir,
                      const std::string& err_string) {
  EXPECT_EQ(new_ir->table_name(), old_ir->table_name()) << err_string;
  EXPECT_EQ(new_ir->IsTimeSet(), old_ir->IsTimeSet()) << err_string;
  EXPECT_EQ(new_ir->time_start_ns(), old_ir->time_start_ns()) << err_string;
  EXPECT_EQ(new_ir->time_stop_ns(), old_ir->time_stop_ns()) << err_string;
  EXPECT_EQ(new_ir->column_names(), old_ir->column_names()) << err_string;
  EXPECT_EQ(new_ir->column_index_map_set(), old_ir->column_index_map_set()) << err_string;
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
  EXPECT_EQ(new_ir->func_name(), old_ir->func_name()) << err_string;
  EXPECT_EQ(new_ir->op().op_code, old_ir->op().op_code) << err_string;
  EXPECT_EQ(new_ir->op().python_op, old_ir->op().python_op) << err_string;
  EXPECT_EQ(new_ir->op().carnot_op_name, old_ir->op().carnot_op_name) << err_string;
  EXPECT_EQ(new_ir->func_id(), old_ir->func_id()) << err_string;
  EXPECT_EQ(new_ir->IsDataTypeEvaluated(), old_ir->IsDataTypeEvaluated()) << err_string;
  EXPECT_EQ(new_ir->EvaluatedDataType(), old_ir->EvaluatedDataType()) << err_string;

  std::vector<ExpressionIR*> new_args = new_ir->args();
  std::vector<ExpressionIR*> old_args = old_ir->args();
  ASSERT_EQ(new_args.size(), old_args.size()) << err_string;
  for (size_t i = 0; i < new_args.size(); ++i) {
    CompareClone(new_args[i], old_args[i], err_string);
  }
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
  EXPECT_EQ(new_ir->destination_id(), old_ir->destination_id()) << err_string;
  EXPECT_EQ(new_ir->destination_address(), old_ir->destination_address()) << err_string;
  EXPECT_EQ(new_ir->DestinationAddressSet(), old_ir->DestinationAddressSet()) << err_string;
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
    CompareClone(output_columns_new[i], output_columns_old[i],
                 absl::Substitute("$0; in Join operator.", err_string));
  }

  EXPECT_THAT(new_ir->suffix_strs(), ::testing::ElementsAreArray(old_ir->suffix_strs()))
      << err_string;

  auto left_on_columns_new = new_ir->left_on_columns();
  auto left_on_columns_old = old_ir->left_on_columns();
  for (size_t i = 0; i < left_on_columns_new.size(); ++i) {
    CompareClone(left_on_columns_new[i], left_on_columns_old[i],
                 absl::Substitute("$0; in Join operator.", err_string));
  }

  auto right_on_columns_new = new_ir->right_on_columns();
  auto right_on_columns_old = old_ir->right_on_columns();
  for (size_t i = 0; i < right_on_columns_new.size(); ++i) {
    CompareClone(right_on_columns_new[i], right_on_columns_old[i],
                 absl::Substitute("$0; in Join operator.", err_string));
  }
}

void CompareClone(IRNode* new_ir, IRNode* old_ir, const std::string& err_string) {
  ASSERT_NE(new_ir, nullptr);
  ASSERT_NE(old_ir, nullptr);

  if (new_ir->graph_ptr() != old_ir->graph_ptr()) {
    EXPECT_EQ(new_ir->id(), new_ir->id()) << err_string;
  }
  // For all base classes that are derived from IRNode, check their properties here.
  if (Match(new_ir, Expression())) {
    EXPECT_EQ(new_ir->type_string(), old_ir->type_string()) << err_string;
  }
  if (Match(new_ir, Operator())) {
    auto new_op = static_cast<OperatorIR*>(new_ir);
    auto old_op = static_cast<OperatorIR*>(old_ir);
    // Check relation status.
    EXPECT_EQ(new_op->IsRelationInit(), old_op->IsRelationInit());
    EXPECT_EQ(new_op->relation().col_names(), old_op->relation().col_names());
    EXPECT_EQ(new_op->relation().col_types(), old_op->relation().col_types());

    // Check parents.
    ASSERT_EQ(new_op->parents().size(), old_op->parents().size());
    for (size_t parent_idx = 0; parent_idx < new_op->parents().size(); ++parent_idx) {
      CompareClone(new_op->parents()[parent_idx], old_op->parents()[parent_idx], err_string);
    }
  }

  EXPECT_EQ(new_ir->type(), old_ir->type()) << err_string;
  switch (new_ir->type()) {
    case IRNodeType::kColumn:
      return CompareCloneNode(static_cast<ColumnIR*>(new_ir), static_cast<ColumnIR*>(old_ir),
                              err_string);
    case IRNodeType::kInt:
      return CompareCloneNode(static_cast<IntIR*>(new_ir), static_cast<IntIR*>(old_ir), err_string);
    case IRNodeType::kString:
      return CompareCloneNode(static_cast<StringIR*>(new_ir), static_cast<StringIR*>(old_ir),
                              err_string);
    case IRNodeType::kMap:
      return CompareCloneNode(static_cast<MapIR*>(new_ir), static_cast<MapIR*>(old_ir), err_string);
    case IRNodeType::kBlockingAgg:
      return CompareCloneNode(static_cast<BlockingAggIR*>(new_ir),
                              static_cast<BlockingAggIR*>(old_ir), err_string);
    case IRNodeType::kGroupBy:
      return CompareCloneNode(static_cast<GroupByIR*>(new_ir), static_cast<GroupByIR*>(old_ir),
                              err_string);
    case IRNodeType::kMetadata:
      return CompareCloneNode(static_cast<MetadataIR*>(new_ir), static_cast<MetadataIR*>(old_ir),
                              err_string);
    case IRNodeType::kMetadataLiteral:
      return CompareCloneNode(static_cast<MetadataLiteralIR*>(new_ir),
                              static_cast<MetadataLiteralIR*>(old_ir), err_string);
    case IRNodeType::kMemorySource:
      return CompareCloneNode(static_cast<MemorySourceIR*>(new_ir),
                              static_cast<MemorySourceIR*>(old_ir), err_string);
    case IRNodeType::kMemorySink:
      return CompareCloneNode(static_cast<MemorySinkIR*>(new_ir),
                              static_cast<MemorySinkIR*>(old_ir), err_string);
    case IRNodeType::kFilter:
      return CompareCloneNode(static_cast<FilterIR*>(new_ir), static_cast<FilterIR*>(old_ir),
                              err_string);
    case IRNodeType::kLimit:
      return CompareCloneNode(static_cast<LimitIR*>(new_ir), static_cast<LimitIR*>(old_ir),
                              err_string);
    case IRNodeType::kFunc:
      return CompareCloneNode(static_cast<FuncIR*>(new_ir), static_cast<FuncIR*>(old_ir),
                              err_string);
    case IRNodeType::kGRPCSourceGroup:
      return CompareCloneNode(static_cast<GRPCSourceGroupIR*>(new_ir),
                              static_cast<GRPCSourceGroupIR*>(old_ir), err_string);
    case IRNodeType::kGRPCSource:
      return CompareCloneNode(static_cast<GRPCSourceIR*>(new_ir),
                              static_cast<GRPCSourceIR*>(old_ir), err_string);
    case IRNodeType::kGRPCSink:
      return CompareCloneNode(static_cast<GRPCSinkIR*>(new_ir), static_cast<GRPCSinkIR*>(old_ir),
                              err_string);
    case IRNodeType::kJoin:
      return CompareCloneNode(static_cast<JoinIR*>(new_ir), static_cast<JoinIR*>(old_ir),
                              err_string);
    default:
      EXPECT_TRUE(false) << absl::Substitute("No compare func exists in test for $0",
                                             new_ir->DebugString());
  }
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
