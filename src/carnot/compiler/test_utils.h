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
#include "src/carnot/compiler/parser/string_reader.h"
#include "src/common/testing/testing.h"

namespace pl {
namespace carnot {
namespace compiler {
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
  PL_RETURN_IF_ERROR(info->Init(info_pb));
  auto compiler_state =
      std::make_shared<CompilerState>(std::make_unique<RelationMap>(), info.get(), 0);
  ASTVisitorImpl ast_walker(ir.get(), compiler_state.get());

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
    PL_RETURN_IF_ERROR(ast_walker.ProcessModuleNode(ast));
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
    MemorySourceIR* mem_source = graph->MakeNode<MemorySourceIR>(ast).ValueOrDie();
    PL_CHECK_OK(mem_source->Init(name, {}));
    return mem_source;
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

  MapIR* MakeMap(OperatorIR* parent, const ColExpressionVector& col_map) {
    MapIR* map = graph->MakeNode<MapIR>(ast).ConsumeValueOrDie();
    PL_CHECK_OK(map->Init(parent, col_map));
    return map;
  }

  // TODO(philkuz) deprecate this when we remove Lambda support.
  LambdaIR* MakeLambda(ExpressionIR* expr, int64_t num_parents) {
    return MakeLambda(expr, {}, num_parents);
  }

  LambdaIR* MakeLambda(ExpressionIR* expr, const std::unordered_set<std::string>& expected_cols,
                       int64_t num_parents) {
    LambdaIR* lambda = graph->MakeNode<LambdaIR>(ast).ConsumeValueOrDie();
    PL_CHECK_OK(lambda->Init(expected_cols, expr, num_parents));
    return lambda;
  }

  LambdaIR* MakeLambda(const ColExpressionVector& expr,
                       const std::unordered_set<std::string>& expected_cols, int64_t num_parents) {
    LambdaIR* lambda = graph->MakeNode<LambdaIR>(ast).ConsumeValueOrDie();
    PL_CHECK_OK(lambda->Init(expected_cols, expr, num_parents));
    return lambda;
  }

  LambdaIR* MakeLambda(const ColExpressionVector& expr, int64_t num_parents) {
    return MakeLambda(expr, {}, num_parents);
  }

  MemorySinkIR* MakeMemSink(OperatorIR* parent, std::string name,
                            const std::vector<std::string>& out_cols) {
    auto sink = graph->MakeNode<MemorySinkIR>(ast).ValueOrDie();
    PL_CHECK_OK(sink->Init(parent, name, out_cols));
    return sink;
  }

  MemorySinkIR* MakeMemSink(OperatorIR* parent, std::string name) {
    return MakeMemSink(parent, name, {});
  }

  FilterIR* MakeFilter(OperatorIR* parent, ExpressionIR* filter_expr) {
    FilterIR* filter = graph->MakeNode<FilterIR>(ast).ValueOrDie();
    EXPECT_OK(filter->Init(parent, filter_expr));
    return filter;
  }

  LimitIR* MakeLimit(OperatorIR* parent, int64_t limit_value) {
    LimitIR* limit = graph->MakeNode<LimitIR>(ast).ValueOrDie();
    EXPECT_OK(limit->Init(parent, limit_value));
    return limit;
  }

  BlockingAggIR* MakeBlockingAgg(OperatorIR* parent, const std::vector<ColumnIR*>& columns,
                                 const ColExpressionVector& col_agg) {
    BlockingAggIR* agg = graph->MakeNode<BlockingAggIR>(ast).ConsumeValueOrDie();
    PL_CHECK_OK(agg->Init(parent, columns, col_agg));
    return agg;
  }

  ColumnIR* MakeColumn(const std::string& name, int64_t parent_op_idx) {
    ColumnIR* column = graph->MakeNode<ColumnIR>(ast).ValueOrDie();
    PL_CHECK_OK(column->Init(name, parent_op_idx, ast));
    return column;
  }

  ColumnIR* MakeColumn(const std::string& name, int64_t parent_op_idx,
                       const table_store::schema::Relation& relation) {
    ColumnIR* column = MakeColumn(name, parent_op_idx);
    column->ResolveColumn(relation.GetColumnIndex(name), relation.GetColumnType(name));
    return column;
  }

  StringIR* MakeString(std::string val) {
    auto str_ir = graph->MakeNode<StringIR>(ast).ValueOrDie();
    EXPECT_OK(str_ir->Init(val, ast));
    return str_ir;
  }

  IntIR* MakeInt(int64_t val) {
    auto int_ir = graph->MakeNode<IntIR>(ast).ValueOrDie();
    EXPECT_OK(int_ir->Init(val, ast));
    return int_ir;
  }

  FuncIR* MakeAddFunc(ExpressionIR* left, ExpressionIR* right) {
    FuncIR* func = graph->MakeNode<FuncIR>(ast).ValueOrDie();
    PL_CHECK_OK(func->Init({FuncIR::Opcode::add, "+", "add"},
                           std::vector<ExpressionIR*>({left, right}), ast));
    return func;
  }
  FuncIR* MakeSubFunc(ExpressionIR* left, ExpressionIR* right) {
    FuncIR* func = graph->MakeNode<FuncIR>(ast).ValueOrDie();
    PL_CHECK_OK(func->Init(FuncIR::op_map.find("-")->second,
                           std::vector<ExpressionIR*>({left, right}), ast));
    return func;
  }

  FuncIR* MakeEqualsFunc(ExpressionIR* left, ExpressionIR* right) {
    FuncIR* func = graph->MakeNode<FuncIR>(ast).ValueOrDie();
    PL_CHECK_OK(func->Init({FuncIR::Opcode::eq, "==", "equals"},
                           std::vector<ExpressionIR*>({left, right}), ast));
    return func;
  }

  FuncIR* MakeFunc(const std::string& name, const std::vector<ExpressionIR*>& args) {
    FuncIR* func = graph->MakeNode<FuncIR>(ast).ValueOrDie();
    PL_CHECK_OK(func->Init({FuncIR::Opcode::non_op, "", name}, args, ast));
    return func;
  }

  FuncIR* MakeAndFunc(ExpressionIR* left, ExpressionIR* right) {
    FuncIR* func = graph->MakeNode<FuncIR>(ast).ValueOrDie();
    PL_CHECK_OK(func->Init(FuncIR::op_map.find("and")->second,
                           std::vector<ExpressionIR*>({left, right}), ast));
    return func;
  }

  MetadataIR* MakeMetadataIR(const std::string& name, int64_t parent_op_idx) {
    MetadataIR* metadata = graph->MakeNode<MetadataIR>(ast).ValueOrDie();
    PL_CHECK_OK(metadata->Init(name, parent_op_idx, ast));
    return metadata;
  }

  MetadataLiteralIR* MakeMetadataLiteral(DataIR* data_ir) {
    MetadataLiteralIR* metadata_literal = graph->MakeNode<MetadataLiteralIR>(ast).ValueOrDie();
    PL_CHECK_OK(metadata_literal->Init(data_ir, ast));
    return metadata_literal;
  }

  FuncIR* MakeMeanFunc(ExpressionIR* value) {
    FuncIR* func = graph->MakeNode<FuncIR>(ast).ValueOrDie();
    PL_CHECK_OK(
        func->Init({FuncIR::Opcode::non_op, "", "mean"}, std::vector<ExpressionIR*>({value}), ast));
    return func;
  }
  FuncIR* MakeMeanFunc() {
    FuncIR* func = graph->MakeNode<FuncIR>(ast).ValueOrDie();
    PL_CHECK_OK(func->Init({FuncIR::Opcode::non_op, "", "mean"}, {}, ast));
    return func;
  }

  std::shared_ptr<IR> SwapGraphBeingBuilt(std::shared_ptr<IR> new_graph) {
    std::shared_ptr<IR> old_graph = graph;
    graph = new_graph;
    return old_graph;
  }

  GRPCSourceGroupIR* MakeGRPCSourceGroup(int64_t source_id,
                                         const table_store::schema::Relation& relation) {
    GRPCSourceGroupIR* grpc_src_group = graph->MakeNode<GRPCSourceGroupIR>(ast).ValueOrDie();
    EXPECT_OK(grpc_src_group->Init(source_id, relation));
    return grpc_src_group;
  }

  GRPCSinkIR* MakeGRPCSink(OperatorIR* parent, int64_t source_id) {
    GRPCSinkIR* grpc_sink = graph->MakeNode<GRPCSinkIR>(ast).ValueOrDie();
    EXPECT_OK(grpc_sink->Init(parent, source_id));
    return grpc_sink;
  }

  GRPCSourceIR* MakeGRPCSource(const std::string& source_id,
                               const table_store::schema::Relation& relation) {
    GRPCSourceIR* grpc_src_group = graph->MakeNode<GRPCSourceIR>(ast).ValueOrDie();
    EXPECT_OK(grpc_src_group->Init(source_id, relation));
    return grpc_src_group;
  }

  UnionIR* MakeUnion(std::vector<OperatorIR*> parents) {
    UnionIR* union_node = graph->MakeNode<UnionIR>(ast).ValueOrDie();
    EXPECT_OK(union_node->Init(parents));
    return union_node;
  }

  JoinIR* MakeJoin(const std::vector<OperatorIR*>& parents, const std::string& join_type,
                   ExpressionIR* equality_condition, const ColExpressionVector& output_columns) {
    // t1.Join(type="inner", cond=lambda a,b: a.col1 == b.col2, cols = lambda a,b:{
    // "col1", a.col1,
    // "col2", b.col2})

    JoinIR* join_node = graph->MakeNode<JoinIR>(ast).ConsumeValueOrDie();

    auto eq_conditions = JoinIR::ParseCondition(equality_condition).ConsumeValueOrDie();
    PL_CHECK_OK(join_node->Init(parents, join_type, eq_conditions.left_on_cols,
                                eq_conditions.right_on_cols, {}));

    std::vector<std::string> output_column_names;
    std::vector<ColumnIR*> output_column_values;
    for (const auto& [name, node] : output_columns) {
      output_column_names.push_back(name);
      CHECK(Match(node, ColumnNode()));
      output_column_values.push_back(static_cast<ColumnIR*>(node));
    }

    PL_CHECK_OK(join_node->SetOutputColumns(output_column_names, output_column_values));
    return join_node;
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
    TabletSourceGroupIR* group = graph->MakeNode<TabletSourceGroupIR>(ast).ConsumeValueOrDie();
    PL_CHECK_OK(group->Init(mem_source, tablet_key_values, tablet_key));
    return group;
  }

  GroupByIR* MakeGroupBy(OperatorIR* parent, const std::vector<ColumnIR*>& groups) {
    GroupByIR* groupby = graph->MakeNode<GroupByIR>(ast).ConsumeValueOrDie();
    PL_CHECK_OK(groupby->Init(parent, groups));
    return groupby;
  }

  template <typename... Args>
  ListIR* MakeList(Args... args) {
    ListIR* list = graph->MakeNode<ListIR>(ast).ConsumeValueOrDie();
    PL_CHECK_OK(list->Init(ast, std::vector<ExpressionIR*>{args...}));
    return list;
  }

  template <typename... Args>
  TupleIR* MakeTuple(Args... args) {
    TupleIR* tuple = graph->MakeNode<TupleIR>(ast).ConsumeValueOrDie();
    PL_CHECK_OK(tuple->Init(ast, std::vector<ExpressionIR*>{args...}));
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

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
