#pragma once

#include <gmock/gmock.h>
#include <utility>
#include <vector>

#include <cstdio>
#include <fstream>
#include <memory>
#include <string>

#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/ast_visitor.h"
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/string_reader.h"

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
  ASTWalker ast_walker(ir, compiler_state.get());

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
  explicit CompilerErrorMatcher(std::string text_pb)
      : expected_compiler_error_(std::move(text_pb)) {}

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
    for (int64_t i = 0; i < error_group.errors_size(); i++) {
      auto error = error_group.errors(i).line_col_error();
      std::string msg = error.message();
      if (msg == expected_compiler_error_) {
        return true;
      }
      error_messages.push_back(msg);
    }
    (*listener) << absl::Substitute("Expected compiler error '$0' not found. Have '$1'",
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

  MemorySourceIR* MakeMemSource() {
    MemorySourceIR* mem_source = graph->MakeNode<MemorySourceIR>().ValueOrDie();
    PL_CHECK_OK(
        mem_source->Init(nullptr, {{"table", MakeString("table")}, {"select", nullptr}}, ast));
    return mem_source;
  }

  MemorySourceIR* MakeMemSource(const Relation& relation) {
    MemorySourceIR* mem_source = MakeMemSource();
    EXPECT_OK(mem_source->SetRelation(relation));
    std::vector<ColumnIR*> columns;
    for (size_t i = 0; i < relation.NumColumns(); ++i) {
      ColumnIR* col = MakeColumn(relation.col_names()[i], 0);
      col->ResolveColumn(i, relation.col_types()[i]);
      columns.push_back(col);
    }
    PL_CHECK_OK(mem_source->SetColumns(columns));
    return mem_source;
  }

  MapIR* MakeMap(OperatorIR* parent, const ColExpressionVector& col_map) {
    MapIR* map = graph->MakeNode<MapIR>().ConsumeValueOrDie();
    LambdaIR* lambda = graph->MakeNode<LambdaIR>().ConsumeValueOrDie();
    PL_CHECK_OK(lambda->Init({}, col_map, ast));
    PL_CHECK_OK(map->Init(parent, {{"fn", lambda}}, ast));
    return map;
  }

  MemorySinkIR* MakeMemSink(OperatorIR* parent, std::string name) {
    auto sink = graph->MakeNode<MemorySinkIR>().ValueOrDie();
    PL_CHECK_OK(sink->Init(parent, {{"name", MakeString(name)}}, ast));
    return sink;
  }

  FilterIR* MakeFilter(OperatorIR* parent, ExpressionIR* filter_expr) {
    auto filter_func_lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
    EXPECT_OK(filter_func_lambda->Init({}, filter_expr, ast));

    FilterIR* filter = graph->MakeNode<FilterIR>().ValueOrDie();
    ArgMap amap({{"fn", filter_func_lambda}});
    EXPECT_OK(filter->Init(parent, amap, ast));
    return filter;
  }

  LimitIR* MakeLimit(OperatorIR* parent, int64_t limit_value) {
    LimitIR* limit = graph->MakeNode<LimitIR>().ValueOrDie();
    ArgMap amap({{"rows", MakeInt(limit_value)}});
    EXPECT_OK(limit->Init(parent, amap, ast));
    return limit;
  }

  BlockingAggIR* MakeBlockingAgg(OperatorIR* parent, const std::vector<ColumnIR*>& columns,
                                 const ColExpressionVector& col_agg) {
    BlockingAggIR* agg = graph->MakeNode<BlockingAggIR>().ConsumeValueOrDie();
    LambdaIR* fn_lambda = graph->MakeNode<LambdaIR>().ConsumeValueOrDie();
    PL_CHECK_OK(fn_lambda->Init({}, col_agg, ast));
    ListIR* list_ir = graph->MakeNode<ListIR>().ConsumeValueOrDie();
    std::vector<ExpressionIR*> exprs;
    for (auto c : columns) {
      exprs.push_back(c);
    }
    PL_CHECK_OK(list_ir->Init(ast, exprs));
    LambdaIR* by_lambda = graph->MakeNode<LambdaIR>().ConsumeValueOrDie();
    PL_CHECK_OK(by_lambda->Init({}, list_ir, ast));
    PL_CHECK_OK(agg->Init(parent, {{"by", by_lambda}, {"fn", fn_lambda}}, ast));
    return agg;
  }

  ColumnIR* MakeColumn(const std::string& name, int64_t parent_op_idx) {
    ColumnIR* column = graph->MakeNode<ColumnIR>().ValueOrDie();
    PL_CHECK_OK(column->Init(name, parent_op_idx, ast));
    return column;
  }

  ColumnIR* MakeColumn(const std::string& name, int64_t parent_op_idx, const Relation& relation) {
    ColumnIR* column = MakeColumn(name, parent_op_idx);
    column->ResolveColumn(relation.GetColumnIndex(name), relation.GetColumnType(name));
    return column;
  }

  StringIR* MakeString(std::string val) {
    auto str_ir = graph->MakeNode<StringIR>().ValueOrDie();
    EXPECT_OK(str_ir->Init(val, ast));
    return str_ir;
  }

  IntIR* MakeInt(int64_t val) {
    auto int_ir = graph->MakeNode<IntIR>().ValueOrDie();
    EXPECT_OK(int_ir->Init(val, ast));
    return int_ir;
  }

  FuncIR* MakeAddFunc(ExpressionIR* left, ExpressionIR* right) {
    FuncIR* func = graph->MakeNode<FuncIR>().ValueOrDie();
    PL_CHECK_OK(func->Init({FuncIR::Opcode::add, "+", "add"}, ASTWalker::kRunTimeFuncPrefix,
                           std::vector<ExpressionIR*>({left, right}), false /* compile_time */,
                           ast));
    return func;
  }

  FuncIR* MakeEqualsFunc(ExpressionIR* left, ExpressionIR* right) {
    FuncIR* func = graph->MakeNode<FuncIR>().ValueOrDie();
    PL_CHECK_OK(func->Init({FuncIR::Opcode::eq, "==", "equals"}, ASTWalker::kRunTimeFuncPrefix,
                           std::vector<ExpressionIR*>({left, right}), false /* compile_time */,
                           ast));
    return func;
  }

  FuncIR* MakeAndFunc(ExpressionIR* left, ExpressionIR* right) {
    FuncIR* func = graph->MakeNode<FuncIR>().ValueOrDie();
    PL_CHECK_OK(func->Init(FuncIR::op_map.find("and")->second, ASTWalker::kRunTimeFuncPrefix,
                           std::vector<ExpressionIR*>({left, right}), false /* compile_time */,
                           ast));
    return func;
  }

  MetadataIR* MakeMetadataIR(const std::string& name, int64_t parent_op_idx) {
    MetadataIR* metadata = graph->MakeNode<MetadataIR>().ValueOrDie();
    PL_CHECK_OK(metadata->Init(name, parent_op_idx, ast));
    return metadata;
  }

  MetadataLiteralIR* MakeMetadataLiteral(DataIR* data_ir) {
    MetadataLiteralIR* metadata_literal = graph->MakeNode<MetadataLiteralIR>().ValueOrDie();
    PL_CHECK_OK(metadata_literal->Init(data_ir, ast));
    return metadata_literal;
  }

  FuncIR* MakeMeanFunc(ExpressionIR* value) {
    FuncIR* func = graph->MakeNode<FuncIR>().ValueOrDie();
    PL_CHECK_OK(func->Init({FuncIR::Opcode::non_op, "", "mean"}, ASTWalker::kRunTimeFuncPrefix,
                           std::vector<ExpressionIR*>({value}), false /* compile_time */, ast));
    return func;
  }

  std::shared_ptr<IR> SwapGraphBeingBuilt(std::shared_ptr<IR> new_graph) {
    std::shared_ptr<IR> old_graph = graph;
    graph = new_graph;
    return old_graph;
  }

  GRPCSourceGroupIR* MakeGRPCSourceGroup(int64_t source_id, const Relation& relation) {
    GRPCSourceGroupIR* grpc_src_group = graph->MakeNode<GRPCSourceGroupIR>().ValueOrDie();
    EXPECT_OK(grpc_src_group->Init(source_id, relation, ast));
    return grpc_src_group;
  }

  GRPCSinkIR* MakeGRPCSink(OperatorIR* parent, int64_t source_id) {
    GRPCSinkIR* grpc_sink = graph->MakeNode<GRPCSinkIR>().ValueOrDie();
    EXPECT_OK(grpc_sink->Init(parent, source_id, ast));
    return grpc_sink;
  }

  GRPCSourceIR* MakeGRPCSource(const std::string& source_id, const Relation& relation) {
    GRPCSourceIR* grpc_src_group = graph->MakeNode<GRPCSourceIR>().ValueOrDie();
    EXPECT_OK(grpc_src_group->Init(source_id, relation, ast));
    return grpc_src_group;
  }

  UnionIR* MakeUnion(std::vector<OperatorIR*> parents) {
    UnionIR* union_node = graph->MakeNode<UnionIR>().ValueOrDie();
    EXPECT_OK(union_node->Init(parents, {{}}, ast));
    return union_node;
  }

  JoinIR* MakeJoin(const std::vector<OperatorIR*>& parents, const std::string& join_type,
                   ExpressionIR* equality_condition, const ColExpressionVector& output_columns) {
    // t1.Join(type="inner", cond=lambda a,b: a.col1 == b.col2, cols = lambda a,b:{
    // "col1", a.col1,
    // "col2", b.col2})

    JoinIR* join_node = graph->MakeNode<JoinIR>().ValueOrDie();
    LambdaIR* equality_condition_lambda = graph->MakeNode<LambdaIR>().ConsumeValueOrDie();
    PL_CHECK_OK(equality_condition_lambda->Init({}, equality_condition, ast));

    LambdaIR* output_columns_lambda = graph->MakeNode<LambdaIR>().ConsumeValueOrDie();
    PL_CHECK_OK(output_columns_lambda->Init({}, output_columns, ast));

    PL_CHECK_OK(join_node->Init(parents,
                                {{"type", MakeString(join_type)},
                                 {"cond", equality_condition_lambda},
                                 {"cols", output_columns_lambda}},
                                ast));
    return join_node;
  }

  // Use this if you need a relation but don't care about the contents.
  Relation MakeRelation() {
    return table_store::schema::Relation(
        std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64,
                                      types::DataType::FLOAT64, types::DataType::FLOAT64}),
        std::vector<std::string>({"count", "cpu0", "cpu1", "cpu2"}));
  }

  pypa::AstPtr ast;
  std::shared_ptr<IR> graph;
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
