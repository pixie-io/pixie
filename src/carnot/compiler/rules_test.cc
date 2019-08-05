#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/metadata_handler.h"
#include "src/carnot/compiler/rule_mock.h"
#include "src/carnot/compiler/rules.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"

namespace pl {
namespace carnot {
namespace compiler {

using testing::_;

const char* kExtraScalarUDFs = R"proto(
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
  name: "pl.upid_to_pod_id"
  exec_arg_types: UINT128
  return_type: STRING
}
scalar_udfs {
  name: "pl.upid_to_pod_name"
  exec_arg_types: UINT128
  return_type: STRING
}
scalar_udfs {
  name: "pl.pod_id_to_pod_name"
  exec_arg_types: STRING
  return_type: STRING
}
)proto";
class RulesTest : public ::testing::Test {
 protected:
  void SetUpRegistryInfo() {
    // TODO(philkuz) replace the following call info_
    // info_ = udfexporter::ExportUDFInfo().ConsumeValueOrDie();
    auto scalar_udf_registry = std::make_unique<udf::ScalarUDFRegistry>("udf_registry");
    auto uda_registry = std::make_unique<udf::UDARegistry>("uda_registry");
    builtins::RegisterBuiltinsOrDie(scalar_udf_registry.get());
    builtins::RegisterBuiltinsOrDie(uda_registry.get());
    auto udf_proto = udf::RegistryInfoExporter()
                         .Registry(*uda_registry)
                         .Registry(*scalar_udf_registry)
                         .ToProto();

    std::string new_udf_info = absl::Substitute("$0$1", udf_proto.DebugString(), kExtraScalarUDFs);
    google::protobuf::TextFormat::MergeFromString(new_udf_info, &udf_proto);

    info_ = std::make_unique<compiler::RegistryInfo>();
    PL_CHECK_OK(info_->Init(udf_proto));
  }
  void SetUp() override {
    ::testing::Test::SetUp();
    SetUpRegistryInfo();

    auto rel_map = std::make_unique<RelationMap>();
    cpu_relation = table_store::schema::Relation(
        std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64,
                                      types::DataType::FLOAT64, types::DataType::FLOAT64}),
        std::vector<std::string>({"count", "cpu0", "cpu1", "cpu2"}));
    rel_map->emplace("cpu", cpu_relation);

    compiler_state_ = std::make_unique<CompilerState>(std::move(rel_map), info_.get(), time_now);
    ast = MakeTestAstPtr();
    graph = std::make_shared<IR>();
    md_handler = MetadataHandler::Create();
  }
  ColumnIR* MakeColumn(const std::string& name, OperatorIR* parent_op) {
    auto column = graph->MakeNode<ColumnIR>().ValueOrDie();
    EXPECT_OK(column->Init(name, parent_op, ast));
    return column;
  }
  MetadataIR* MakeMetadataIR(const std::string& name, OperatorIR* parent_op) {
    auto metadata = graph->MakeNode<MetadataIR>().ValueOrDie();
    EXPECT_OK(metadata->Init(name, parent_op, ast));
    return metadata;
  }
  StringIR* MakeString(const std::string& name) {
    auto string_ir = graph->MakeNode<StringIR>().ValueOrDie();
    EXPECT_OK(string_ir->Init(name, ast));
    return string_ir;
  }

  IntIR* MakeInt(int64_t val) {
    auto int_ir = graph->MakeNode<IntIR>().ValueOrDie();
    EXPECT_OK(int_ir->Init(val, ast));
    return int_ir;
  }

  FuncIR* MakeEqualsFunc(ExpressionIR* left, ExpressionIR* right) {
    FuncIR* func = graph->MakeNode<FuncIR>().ValueOrDie();
    PL_CHECK_OK(func->Init({FuncIR::Opcode::eq, "==", "equals"}, ASTWalker::kRunTimeFuncPrefix,
                           std::vector<ExpressionIR*>({left, right}), false /* compile_time */,
                           ast));
    return func;
  }

  FuncIR* MakeAddFunc(ExpressionIR* left, ExpressionIR* right) {
    FuncIR* func = graph->MakeNode<FuncIR>().ValueOrDie();
    PL_CHECK_OK(func->Init({FuncIR::Opcode::add, "+", "add"}, ASTWalker::kRunTimeFuncPrefix,
                           std::vector<ExpressionIR*>({left, right}), false /* compile_time */,
                           ast));
    return func;
  }

  MetadataResolverIR* MakeMetadataResolver(OperatorIR* parent) {
    MetadataResolverIR* md_resolver = graph->MakeNode<MetadataResolverIR>().ValueOrDie();
    EXPECT_OK(md_resolver->Init(parent, {{}}, ast));
    return md_resolver;
  }
  FilterIR* MakeFilter(OperatorIR* parent) {
    auto constant1 = graph->MakeNode<IntIR>().ValueOrDie();
    EXPECT_OK(constant1->Init(10, ast));

    auto constant2 = graph->MakeNode<IntIR>().ValueOrDie();
    EXPECT_OK(constant2->Init(10, ast));

    auto column = graph->MakeNode<ColumnIR>().ValueOrDie();
    EXPECT_OK(column->Init("column", parent, ast));

    auto filter_func = graph->MakeNode<FuncIR>().ValueOrDie();
    EXPECT_OK(filter_func->Init({FuncIR::Opcode::eq, "==", "equals"}, ASTWalker::kRunTimeFuncPrefix,
                                std::vector<ExpressionIR*>({constant1, column}),
                                false /* compile_time */, ast));
    filter_func->SetOutputDataType(types::DataType::BOOLEAN);

    auto filter_func_lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
    EXPECT_OK(filter_func_lambda->Init({}, filter_func, ast));

    FilterIR* filter = graph->MakeNode<FilterIR>().ValueOrDie();
    ArgMap amap({{"fn", filter_func_lambda}});
    EXPECT_OK(filter->Init(parent, amap, ast));
    return filter;
  }
  FilterIR* MakeFilter(OperatorIR* parent, ColumnIR* filter_value) {
    auto constant1 = graph->MakeNode<StringIR>().ValueOrDie();
    EXPECT_OK(constant1->Init("value", ast));

    FuncIR* filter_func = graph->MakeNode<FuncIR>().ValueOrDie();
    EXPECT_OK(filter_func->Init({FuncIR::Opcode::eq, "==", "equals"}, ASTWalker::kRunTimeFuncPrefix,
                                std::vector<ExpressionIR*>({constant1, filter_value}),
                                false /* compile_time */, ast));

    auto filter_func_lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
    EXPECT_OK(filter_func_lambda->Init({}, filter_func, ast));

    FilterIR* filter = graph->MakeNode<FilterIR>().ValueOrDie();
    ArgMap amap({{"fn", filter_func_lambda}});
    EXPECT_OK(filter->Init(parent, amap, ast));
    return filter;
  }
  BlockingAggIR* MakeBlockingAgg(OperatorIR* parent, ColumnIR* by_column, ColumnIR* fn_column) {
    auto agg_func = graph->MakeNode<FuncIR>().ValueOrDie();
    EXPECT_OK(agg_func->Init({FuncIR::Opcode::non_op, "", "mean"}, ASTWalker::kRunTimeFuncPrefix,
                             std::vector<ExpressionIR*>({fn_column}), false /* compile_time */,
                             ast));

    auto agg_func_lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
    EXPECT_OK(agg_func_lambda->Init({}, {{"agg_fn", agg_func}}, ast));

    auto by_func_lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
    EXPECT_OK(by_func_lambda->Init({"group"}, by_column, ast));

    BlockingAggIR* agg = graph->MakeNode<BlockingAggIR>().ValueOrDie();
    ArgMap amap({{"by", by_func_lambda}, {"fn", agg_func_lambda}});
    EXPECT_OK(agg->Init(parent, amap, ast));
    return agg;
  }

  pypa::AstPtr ast;
  std::shared_ptr<IR> graph;
  std::unique_ptr<CompilerState> compiler_state_;
  std::unique_ptr<RegistryInfo> info_;
  int64_t time_now = 1552607213931245000;
  table_store::schema::Relation cpu_relation;
  std::unique_ptr<MetadataHandler> md_handler;
};
class DataTypeRuleTest : public RulesTest {
 protected:
  void SetUp() override {
    RulesTest::SetUp();
    mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
    PL_CHECK_OK(mem_src->SetRelation(cpu_relation));
  }
  MemorySourceIR* mem_src;
};

// Simple map function.
TEST_F(DataTypeRuleTest, map_function) {
  auto map = graph->MakeNode<MapIR>().ValueOrDie();
  auto constant = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(constant->Init(10, ast));
  auto col = MakeColumn("count", mem_src);
  auto func = graph->MakeNode<FuncIR>().ValueOrDie();
  auto lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
  EXPECT_OK(func->Init({FuncIR::Opcode::add, "+", "add"}, ASTWalker::kRunTimeFuncPrefix,
                       std::vector<ExpressionIR*>({constant, col}), false /* compile_time */, ast));
  EXPECT_OK(lambda->Init({"col_name"}, {{"func", func}}, ast));
  ArgMap amap({{"fn", lambda}});
  EXPECT_OK(map->Init(mem_src, amap, ast));

  // No rule has been run, don't expect any of these to be evaluated.
  EXPECT_FALSE(func->IsDataTypeEvaluated());
  EXPECT_FALSE(col->IsDataTypeEvaluated());

  // Expect the data_rule to change something.
  DataTypeRule data_rule(compiler_state_.get());
  auto result = data_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  // Function shouldn't be updated, it had unresolved dependencies.
  EXPECT_FALSE(func->IsDataTypeEvaluated());
  // Column should be updated, it had unresolved dependencies.
  EXPECT_TRUE(col->IsDataTypeEvaluated());

  // Expect the data_rule to change something.
  result = data_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  // The function should now be evaluated, the column should stay evaluated.
  EXPECT_TRUE(func->IsDataTypeEvaluated());
  EXPECT_TRUE(col->IsDataTypeEvaluated());

  // Both should be integers.
  EXPECT_EQ(col->EvaluatedDataType(), types::DataType::INT64);
  EXPECT_EQ(func->EvaluatedDataType(), types::DataType::INT64);

  // Expect the data_rule to do nothing, no more work left.
  result = data_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());

  // Both should stay evaluated.
  EXPECT_TRUE(func->IsDataTypeEvaluated());
  EXPECT_TRUE(col->IsDataTypeEvaluated());
}

// The DataType shouldn't be resolved for compiler functions. They should be handled with a
// different rule.
TEST_F(DataTypeRuleTest, compiler_function_no_match) {
  // Compiler function should not get resolved.
  auto range = graph->MakeNode<RangeIR>().ValueOrDie();
  auto constant1 = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(constant1->Init(10, ast));
  auto constant2 = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(constant2->Init(12, ast));
  auto constant3 = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(constant3->Init(24, ast));
  auto func2 = graph->MakeNode<FuncIR>().ValueOrDie();
  EXPECT_OK(func2->Init({FuncIR::Opcode::add, "+", "add"}, ASTWalker::kCompileTimeFuncPrefix,
                        std::vector<ExpressionIR*>({constant1, constant2}), true /* compile_time */,
                        ast));
  EXPECT_OK(range->Init(mem_src, func2, constant3, ast));

  // No rule has been run, don't expect any of these to be evaluated.
  EXPECT_FALSE(func2->IsDataTypeEvaluated());
  // Expect the data_rule to do nothing, compiler function shouldn't be matched.
  DataTypeRule data_rule(compiler_state_.get());
  auto result = data_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
  // No rule has been run, don't expect any of these to be evaluated.
  EXPECT_FALSE(func2->IsDataTypeEvaluated());
}

// The DataType shouldn't be resolved for a function without a name.
TEST_F(DataTypeRuleTest, missing_udf_name) {
  auto map = graph->MakeNode<MapIR>().ValueOrDie();
  auto constant = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(constant->Init(10, ast));
  auto col = MakeColumn("count", mem_src);
  auto func = graph->MakeNode<FuncIR>().ValueOrDie();
  auto lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
  EXPECT_OK(func->Init({FuncIR::Opcode::add, "+", "gobeldy"}, ASTWalker::kRunTimeFuncPrefix,
                       std::vector<ExpressionIR*>({constant, col}), false /* compile_time */, ast));
  EXPECT_OK(lambda->Init({"col_name"}, {{"func", func}}, ast));
  ArgMap amap({{"fn", lambda}});
  EXPECT_OK(map->Init(mem_src, amap, ast));

  // Expect the data_rule to successfully change columnir.
  DataTypeRule data_rule(compiler_state_.get());
  auto result = data_rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  // Expect the data_rule to change something.
  result = data_rule.Execute(graph.get());
  EXPECT_NOT_OK(result);

  // The function should not be evaluated, the function was not matched.
  EXPECT_FALSE(func->IsDataTypeEvaluated());
}

// Checks to make sure that agg functions work properly.
TEST_F(DataTypeRuleTest, function_in_agg) {
  auto map = graph->MakeNode<BlockingAggIR>().ValueOrDie();
  auto col = MakeColumn("count", mem_src);
  auto func = graph->MakeNode<FuncIR>().ValueOrDie();
  auto lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
  EXPECT_OK(func->Init({FuncIR::Opcode::non_op, "", "mean"}, ASTWalker::kRunTimeFuncPrefix,
                       std::vector<ExpressionIR*>({col}), false /* compile_time */, ast));
  EXPECT_OK(lambda->Init({col->col_name()}, {{"func", func}}, ast));
  ArgMap amap({{"fn", lambda}, {"by", nullptr}});
  EXPECT_OK(map->Init(mem_src, amap, ast));

  // Expect the data_rule to successfully evaluate the column.
  DataTypeRule data_rule(compiler_state_.get());
  auto result = data_rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_TRUE(col->IsDataTypeEvaluated());
  EXPECT_FALSE(func->IsDataTypeEvaluated());

  // Expect the data_rule to change the function.
  result = data_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  // The function should be evaluated.
  EXPECT_TRUE(func->IsDataTypeEvaluated());
  EXPECT_EQ(func->EvaluatedDataType(), types::DataType::FLOAT64);
}

// Checks to make sure that nested functions are evaluated as expected.
TEST_F(DataTypeRuleTest, nested_functions) {
  auto map = graph->MakeNode<MapIR>().ValueOrDie();
  auto constant = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(constant->Init(10, ast));
  auto constant2 = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(constant2->Init(12, ast));
  auto col = MakeColumn("count", mem_src);
  auto func = graph->MakeNode<FuncIR>().ValueOrDie();
  auto func2 = graph->MakeNode<FuncIR>().ValueOrDie();
  auto lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
  EXPECT_OK(func->Init({FuncIR::Opcode::add, "+", "add"}, ASTWalker::kRunTimeFuncPrefix,
                       std::vector<ExpressionIR*>({constant, col}), false /* compile_time */, ast));

  EXPECT_OK(func2->Init({FuncIR::Opcode::add, "-", "subtract"}, ASTWalker::kRunTimeFuncPrefix,
                        std::vector<ExpressionIR*>({constant2, func}), false /* compile_time */,
                        ast));
  EXPECT_OK(lambda->Init({"col_name"}, {{"col_name", func2}}, ast));
  ArgMap amap({{"fn", lambda}});
  EXPECT_OK(map->Init(mem_src, amap, ast));

  // No rule has been run, don't expect any of these to be evaluated.
  EXPECT_FALSE(func->IsDataTypeEvaluated());
  EXPECT_FALSE(func2->IsDataTypeEvaluated());
  EXPECT_FALSE(col->IsDataTypeEvaluated());

  // Expect the data_rule to change something.
  DataTypeRule data_rule(compiler_state_.get());
  auto result = data_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  // Functions shouldn't be updated, they have unresolved dependencies.
  EXPECT_FALSE(func->IsDataTypeEvaluated());
  EXPECT_FALSE(func2->IsDataTypeEvaluated());
  // Column should be updated, it had no dependencies.
  EXPECT_TRUE(col->IsDataTypeEvaluated());

  // Expect the data_rule to change something.
  result = data_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  // func1 should now be evaluated, the column should stay evaluated, func2 is not evaluated.
  EXPECT_TRUE(func->IsDataTypeEvaluated());
  EXPECT_FALSE(func2->IsDataTypeEvaluated());
  EXPECT_TRUE(col->IsDataTypeEvaluated());

  // Everything should be evaluated, func2 changes.
  result = data_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  // All should be evaluated.
  EXPECT_TRUE(func->IsDataTypeEvaluated());
  EXPECT_TRUE(func2->IsDataTypeEvaluated());
  EXPECT_TRUE(col->IsDataTypeEvaluated());

  // All should be integers.
  EXPECT_EQ(col->EvaluatedDataType(), types::DataType::INT64);
  EXPECT_EQ(func->EvaluatedDataType(), types::DataType::INT64);
  EXPECT_EQ(func2->EvaluatedDataType(), types::DataType::INT64);

  // Expect the data_rule to do nothing, no more work left.
  result = data_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
}

TEST_F(DataTypeRuleTest, metadata_column) {
  MetadataResolverIR* md = MakeMetadataResolver(mem_src);
  std::string metadata_name = "pod_name";
  MetadataProperty* property = md_handler->GetProperty(metadata_name).ValueOrDie();
  table_store::schema::Relation relation({property->column_type()}, {property->GetColumnRepr()});
  EXPECT_OK(md->SetRelation(relation));
  MetadataIR* metadata_ir = MakeMetadataIR(metadata_name, md);
  EXPECT_OK(metadata_ir->ResolveMetadataColumn(md, property));
  MakeFilter(md, metadata_ir);
  EXPECT_FALSE(metadata_ir->IsDataTypeEvaluated());
  // Expect the data_rule to do nothing, no more work left.
  DataTypeRule data_rule(compiler_state_.get());
  auto result = data_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_TRUE(metadata_ir->IsDataTypeEvaluated());
  EXPECT_EQ(metadata_ir->EvaluatedDataType(), types::DataType::STRING);
}

class SourceRelationTest : public RulesTest {
 protected:
  void SetUp() override { RulesTest::SetUp(); }
};

// Simple check with select all.
TEST_F(SourceRelationTest, set_source_select_all) {
  StringIR* table_str_node = graph->MakeNode<StringIR>().ValueOrDie();
  ASSERT_OK(table_str_node->Init("cpu", ast));

  MemorySourceIR* mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  ArgMap memsrc_argmap({{"table", table_str_node}, {"select", nullptr}});
  EXPECT_OK(mem_src->Init(nullptr, memsrc_argmap, ast));

  EXPECT_FALSE(mem_src->IsRelationInit());

  SourceRelationRule source_relation_rule(compiler_state_.get());
  auto result = source_relation_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_TRUE(mem_src->IsRelationInit());
  // Make sure the relations are the same after processing.
  table_store::schema::Relation relation = mem_src->relation();
  EXPECT_TRUE(relation.col_types() == cpu_relation.col_types());
  EXPECT_TRUE(relation.col_names() == cpu_relation.col_names());
}

TEST_F(SourceRelationTest, set_source_variable_columns) {
  std::vector<std::string> str_columns = {"cpu1", "cpu2"};
  StringIR* table_str_node = graph->MakeNode<StringIR>().ValueOrDie();
  std::vector<ExpressionIR*> select_columns;
  for (const std::string& c : str_columns) {
    auto select_col = graph->MakeNode<StringIR>().ValueOrDie();
    EXPECT_OK(select_col->Init(c, ast));
    select_columns.push_back(select_col);
  }
  auto select_list = graph->MakeNode<ListIR>().ValueOrDie();
  EXPECT_OK(select_list->Init(ast, select_columns));
  ASSERT_OK(table_str_node->Init("cpu", ast));

  MemorySourceIR* mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  ArgMap memsrc_argmap({{"table", table_str_node}, {"select", select_list}});
  EXPECT_OK(mem_src->Init(nullptr, memsrc_argmap, ast));

  EXPECT_FALSE(mem_src->IsRelationInit());

  SourceRelationRule source_relation_rule(compiler_state_.get());
  auto result = source_relation_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  EXPECT_TRUE(mem_src->IsRelationInit());
  // Make sure the relations are the same after processing.
  table_store::schema::Relation relation = mem_src->relation();
  auto sub_relation_result = cpu_relation.MakeSubRelation(str_columns);
  EXPECT_OK(sub_relation_result);
  table_store::schema::Relation expected_relation = sub_relation_result.ValueOrDie();
  EXPECT_TRUE(relation.col_types() == expected_relation.col_types());
  EXPECT_TRUE(relation.col_names() == expected_relation.col_names());
}

TEST_F(SourceRelationTest, missing_table_name) {
  StringIR* table_str_node = graph->MakeNode<StringIR>().ValueOrDie();
  std::string table_name = "not_a_real_table_name";
  ASSERT_OK(table_str_node->Init(table_name, ast));

  MemorySourceIR* mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  ArgMap memsrc_argmap({{"table", table_str_node}, {"select", nullptr}});
  EXPECT_OK(mem_src->Init(nullptr, memsrc_argmap, ast));

  EXPECT_FALSE(mem_src->IsRelationInit());

  SourceRelationRule source_relation_rule(compiler_state_.get());
  auto result = source_relation_rule.Execute(graph.get());
  EXPECT_NOT_OK(result);
  std::string error_string = absl::Substitute("Table '$0' not found.", table_name);
  EXPECT_THAT(result.status(), HasCompilerError(error_string));
}

TEST_F(SourceRelationTest, missing_columns) {
  std::string missing_column = "blah_column";
  std::vector<std::string> str_columns = {"cpu1", "cpu2", missing_column};
  StringIR* table_str_node = graph->MakeNode<StringIR>().ValueOrDie();
  std::vector<ExpressionIR*> select_columns;
  for (const std::string& c : str_columns) {
    auto select_col = graph->MakeNode<StringIR>().ValueOrDie();
    EXPECT_OK(select_col->Init(c, ast));
    select_columns.push_back(select_col);
  }
  auto select_list = graph->MakeNode<ListIR>().ValueOrDie();
  EXPECT_OK(select_list->Init(ast, select_columns));
  ASSERT_OK(table_str_node->Init("cpu", ast));

  MemorySourceIR* mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  ArgMap memsrc_argmap({{"table", table_str_node}, {"select", select_list}});
  EXPECT_OK(mem_src->Init(nullptr, memsrc_argmap, ast));

  EXPECT_FALSE(mem_src->IsRelationInit());

  SourceRelationRule source_relation_rule(compiler_state_.get());
  auto result = source_relation_rule.Execute(graph.get());
  EXPECT_NOT_OK(result);
  VLOG(1) << result.status().ToString();

  std::string error_string = absl::Substitute("Columns {$0} are missing in table.", missing_column);
  EXPECT_THAT(result.status(), HasCompilerError(error_string));
}

class BlockingAggRuleTest : public RulesTest {
 protected:
  void SetUp() override { RulesTest::SetUp(); }
  void SetUpGraph(bool resolve_agg_func, bool resolve_agg_group) {
    mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
    PL_CHECK_OK(mem_src->SetRelation(cpu_relation));
    auto constant = graph->MakeNode<IntIR>().ValueOrDie();
    EXPECT_OK(constant->Init(10, ast));

    auto agg_func = graph->MakeNode<FuncIR>().ValueOrDie();
    EXPECT_OK(agg_func->Init({FuncIR::Opcode::non_op, "", "mean"}, ASTWalker::kRunTimeFuncPrefix,
                             std::vector<ExpressionIR*>({constant}), false /* compile_time */,
                             ast));
    if (resolve_agg_func) {
      agg_func->SetOutputDataType(func_data_type);
    }

    auto agg_func_lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
    EXPECT_OK(agg_func_lambda->Init({}, {{agg_func_col, agg_func}}, ast));

    auto by_func_lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
    auto group = MakeColumn(group_name, mem_src);
    // Code to resolve column.
    if (resolve_agg_group) {
      group->ResolveColumn(1, group_data_type);
    }
    EXPECT_OK(by_func_lambda->Init({group_name}, group, ast));

    agg = graph->MakeNode<BlockingAggIR>().ValueOrDie();
    ArgMap amap({{"by", by_func_lambda}, {"fn", agg_func_lambda}});
    ASSERT_OK(agg->Init(mem_src, amap, ast));
  }
  MemorySourceIR* mem_src;
  BlockingAggIR* agg;
  types::DataType func_data_type = types::DataType::FLOAT64;
  types::DataType group_data_type = types::DataType::INT64;
  std::string group_name = "group";
  std::string agg_func_col = "meaned";
};

// Relation should resolve, all expressions in operator are resolved.
TEST_F(BlockingAggRuleTest, successful_resolve) {
  SetUpGraph(true /* resolve_agg_func */, true /* resolve_agg_group */);
  OperatorRelationRule op_rel_rule(compiler_state_.get());
  EXPECT_FALSE(agg->IsRelationInit());
  auto result = op_rel_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  ASSERT_TRUE(agg->IsRelationInit());

  auto result_relation = agg->relation();
  table_store::schema::Relation expected_relation(
      {types::DataType::INT64, types::DataType::FLOAT64}, {group_name, agg_func_col});
  EXPECT_TRUE(result_relation.col_types() == expected_relation.col_types());
  EXPECT_TRUE(result_relation.col_names() == expected_relation.col_names());
}
// Rule shouldn't work because column is not resolved.
TEST_F(BlockingAggRuleTest, failed_resolve_column) {
  SetUpGraph(true /* resolve_agg_func */, false /* resolve_agg_group */);
  OperatorRelationRule op_rel_rule(compiler_state_.get());
  EXPECT_FALSE(agg->IsRelationInit());
  auto result = op_rel_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
  EXPECT_FALSE(agg->IsRelationInit());
}

// Rule shouldn't work because function is not resolved.
TEST_F(BlockingAggRuleTest, failed_resolve_function) {
  SetUpGraph(false /* resolve_agg_func */, true /* resolve_agg_group */);
  OperatorRelationRule op_rel_rule(compiler_state_.get());
  EXPECT_FALSE(agg->IsRelationInit());
  auto result = op_rel_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
  EXPECT_FALSE(agg->IsRelationInit());
}
class MapRuleTest : public RulesTest {
 protected:
  void SetUp() override { RulesTest::SetUp(); }
  void SetUpGraph(bool resolve_map_func) {
    mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
    PL_CHECK_OK(mem_src->SetRelation(cpu_relation));
    auto constant1 = graph->MakeNode<IntIR>().ValueOrDie();
    EXPECT_OK(constant1->Init(10, ast));

    auto constant2 = graph->MakeNode<IntIR>().ValueOrDie();
    EXPECT_OK(constant2->Init(10, ast));

    auto map_func = graph->MakeNode<FuncIR>().ValueOrDie();
    EXPECT_OK(map_func->Init({FuncIR::Opcode::add, "+", "add"}, ASTWalker::kRunTimeFuncPrefix,
                             std::vector<ExpressionIR*>({constant1, constant2}),
                             false /* compile_time */, ast));
    if (resolve_map_func) {
      map_func->SetOutputDataType(func_data_type);
    }

    auto map_func_lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
    EXPECT_OK(map_func_lambda->Init({}, {{map_func_col, map_func}}, ast));

    map = graph->MakeNode<MapIR>().ValueOrDie();
    ArgMap amap({{"fn", map_func_lambda}});
    ASSERT_OK(map->Init(mem_src, amap, ast));
  }
  MemorySourceIR* mem_src;
  MapIR* map;
  types::DataType func_data_type = types::DataType::INT64;
  std::string map_func_col = "sum";
};

// Relation should resolve, all expressions in operator are resolved.
TEST_F(MapRuleTest, successful_resolve) {
  SetUpGraph(true /* resolve_map_func */);
  OperatorRelationRule op_rel_rule(compiler_state_.get());
  EXPECT_FALSE(map->IsRelationInit());
  auto result = op_rel_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  ASSERT_TRUE(map->IsRelationInit());

  auto result_relation = map->relation();
  table_store::schema::Relation expected_relation({types::DataType::INT64}, {map_func_col});
  EXPECT_TRUE(result_relation.col_types() == expected_relation.col_types());
  EXPECT_TRUE(result_relation.col_names() == expected_relation.col_names());
}

// Rule shouldn't work because function is not resolved.
TEST_F(MapRuleTest, failed_resolve_function) {
  SetUpGraph(false /* resolve_map_func */);
  OperatorRelationRule op_rel_rule(compiler_state_.get());
  EXPECT_FALSE(map->IsRelationInit());
  auto result = op_rel_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
  EXPECT_FALSE(map->IsRelationInit());
}

class MetadataResolverRuleTest : public RulesTest {
 protected:
  void SetUp() override { RulesTest::SetUp(); }
  MetadataProperty* GetProperty(const std::string& name) {
    auto property_status = md_handler->GetProperty(name);
    PL_CHECK_OK(property_status);
    return property_status.ValueOrDie();
  }
  void SetUpGraph() {
    mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
    PL_CHECK_OK(mem_src->SetRelation(cpu_relation));

    md_resolver = graph->MakeNode<MetadataResolverIR>().ValueOrDie();
    ArgMap amap({{}});
    PL_CHECK_OK(md_resolver->Init(mem_src, amap, ast));
  }
  MemorySourceIR* mem_src;
  MetadataResolverIR* md_resolver;
  types::DataType func_data_type = types::DataType::INT64;
};

TEST_F(MetadataResolverRuleTest, make_sure_metadata_columns_show_up) {
  SetUpGraph();
  MetadataProperty* service_property = GetProperty("service_name");
  MetadataProperty* pod_property = GetProperty("pod_name");
  PL_CHECK_OK(md_resolver->AddMetadata(service_property));
  PL_CHECK_OK(md_resolver->AddMetadata(pod_property));
  std::vector<types::DataType> expected_col_types = cpu_relation.col_types();
  std::vector<std::string> expected_col_names = cpu_relation.col_names();
  expected_col_names.push_back(pod_property->GetColumnRepr());
  expected_col_names.push_back(service_property->GetColumnRepr());
  expected_col_types.push_back(pod_property->column_type());
  expected_col_types.push_back(service_property->column_type());

  OperatorRelationRule op_rel_rule(compiler_state_.get());
  EXPECT_FALSE(md_resolver->IsRelationInit());
  auto result = op_rel_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());
  ASSERT_TRUE(md_resolver->IsRelationInit());

  auto result_relation = md_resolver->relation();
  EXPECT_TRUE(result_relation.col_types() == expected_col_types);
  EXPECT_TRUE(result_relation.col_names() == expected_col_names);
}

class OperatorRelationTest : public RulesTest {
 protected:
  void SetUp() override {
    RulesTest::SetUp();
    mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
    PL_CHECK_OK(mem_src->SetRelation(cpu_relation));
  }
  LimitIR* MakeLimit(OperatorIR* parent) {
    auto constant1 = graph->MakeNode<IntIR>().ValueOrDie();
    EXPECT_OK(constant1->Init(10, ast));
    LimitIR* limit = graph->MakeNode<LimitIR>().ValueOrDie();
    ArgMap amap({{"rows", constant1}});
    EXPECT_OK(limit->Init(parent, amap, ast));
    return limit;
  }
  MemorySourceIR* mem_src;
};

// Make sure that relations are copied from node to node and the rule will execute consecutively.
TEST_F(OperatorRelationTest, propogate_test) {
  FilterIR* filter = MakeFilter(mem_src);
  LimitIR* limit = MakeLimit(filter);
  EXPECT_FALSE(filter->IsRelationInit());
  EXPECT_FALSE(limit->IsRelationInit());
  OperatorRelationRule op_rel_rule(compiler_state_.get());
  auto result = op_rel_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  // Because limit comes after filter, it can actually evaluate in a single run.
  EXPECT_TRUE(filter->IsRelationInit());
  EXPECT_TRUE(limit->IsRelationInit());

  // Should not have any work left.
  result = op_rel_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
}
class CompilerTimeExpressionTest : public RulesTest {
 protected:
  void SetUp() override {
    RulesTest::SetUp();
    mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
    PL_CHECK_OK(mem_src->SetRelation(cpu_relation));
  }
  FuncIR* MakeConstantAddition(int64_t l, int64_t r) {
    auto constant1 = graph->MakeNode<IntIR>().ValueOrDie();
    EXPECT_OK(constant1->Init(l, ast));

    auto constant2 = graph->MakeNode<IntIR>().ValueOrDie();
    EXPECT_OK(constant2->Init(r, ast));
    auto func = graph->MakeNode<FuncIR>().ValueOrDie();
    EXPECT_OK(func->Init({FuncIR::Opcode::add, "+", "add"}, ASTWalker::kCompileTimeFuncPrefix,
                         std::vector<ExpressionIR*>({constant1, constant2}),
                         true /* compile_time */, ast));
    return func;
  }
  RangeIR* MakeRange(IRNode* start, IRNode* stop) {
    RangeIR* range = graph->MakeNode<RangeIR>().ValueOrDie();
    EXPECT_OK(range->Init(mem_src, start, stop, ast));
    return range;
  }
  MemorySourceIR* mem_src;
};

TEST_F(CompilerTimeExpressionTest, one_argument_function) {
  auto start = MakeConstantAddition(4, 6);
  auto stop = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(stop->Init(13, ast));

  RangeIR* range = MakeRange(start, stop);
  RangeArgExpressionRule compiler_expr_rule(compiler_state_.get());

  auto result = compiler_expr_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  IRNode* res_start_repr = range->start_repr();
  IRNode* res_stop_repr = range->stop_repr();
  ASSERT_EQ(res_start_repr->type(), IRNodeType::kInt);
  ASSERT_EQ(res_stop_repr->type(), IRNodeType::kInt);
  EXPECT_EQ(static_cast<IntIR*>(res_start_repr)->val(), 10);
  // Make sure that we don't manipulate the start value.
  EXPECT_EQ(static_cast<IntIR*>(res_stop_repr)->val(), 13);
}

TEST_F(CompilerTimeExpressionTest, two_argument_function) {
  RangeIR* range = MakeRange(MakeConstantAddition(4, 6), MakeConstantAddition(123, 321));
  RangeArgExpressionRule compiler_expr_rule(compiler_state_.get());

  auto result = compiler_expr_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  IRNode* res_start_repr = range->start_repr();
  IRNode* res_stop_repr = range->stop_repr();
  ASSERT_EQ(res_start_repr->type(), IRNodeType::kInt);
  ASSERT_EQ(res_stop_repr->type(), IRNodeType::kInt);
  EXPECT_EQ(static_cast<IntIR*>(res_start_repr)->val(), 10);
  EXPECT_EQ(static_cast<IntIR*>(res_stop_repr)->val(), 444);
}

TEST_F(CompilerTimeExpressionTest, one_argument_string) {
  int64_t num_minutes_ago = 2;
  std::chrono::nanoseconds exp_time = std::chrono::minutes(num_minutes_ago);
  int64_t expected_time = time_now - exp_time.count();
  std::string stop_str_repr = absl::Substitute("-$0m", num_minutes_ago);

  auto stop = graph->MakeNode<StringIR>().ValueOrDie();
  EXPECT_OK(stop->Init(stop_str_repr, ast));

  auto start = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(start->Init(10, ast));

  RangeIR* range = MakeRange(start, stop);
  RangeArgExpressionRule compiler_expr_rule(compiler_state_.get());

  auto result = compiler_expr_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  IRNode* res_start_repr = range->start_repr();
  IRNode* res_stop_repr = range->stop_repr();
  ASSERT_EQ(res_start_repr->type(), IRNodeType::kInt);
  ASSERT_EQ(res_stop_repr->type(), IRNodeType::kInt);
  // Make sure that we don't manipulate the start value.
  EXPECT_EQ(static_cast<IntIR*>(res_start_repr)->val(), 10);
  EXPECT_EQ(static_cast<IntIR*>(res_stop_repr)->val(), expected_time);
}

TEST_F(CompilerTimeExpressionTest, two_argument_string) {
  int64_t start_num_minutes_ago = 2;
  int64_t stop_num_minutes_ago = 1;
  std::chrono::nanoseconds exp_stop_time = std::chrono::minutes(stop_num_minutes_ago);
  int64_t expected_stop_time = time_now - exp_stop_time.count();
  std::string stop_str_repr = absl::Substitute("-$0m", stop_num_minutes_ago);

  std::chrono::nanoseconds exp_start_time = std::chrono::minutes(start_num_minutes_ago);
  int64_t expected_start_time = time_now - exp_start_time.count();
  std::string start_str_repr = absl::Substitute("-$0m", start_num_minutes_ago);

  auto start = graph->MakeNode<StringIR>().ValueOrDie();
  EXPECT_OK(start->Init(start_str_repr, ast));

  auto stop = graph->MakeNode<StringIR>().ValueOrDie();
  EXPECT_OK(stop->Init(stop_str_repr, ast));

  RangeIR* range = MakeRange(start, stop);
  RangeArgExpressionRule compiler_expr_rule(compiler_state_.get());

  auto result = compiler_expr_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  IRNode* res_start_repr = range->start_repr();
  IRNode* res_stop_repr = range->stop_repr();
  ASSERT_EQ(res_start_repr->type(), IRNodeType::kInt);
  ASSERT_EQ(res_stop_repr->type(), IRNodeType::kInt);
  EXPECT_EQ(static_cast<IntIR*>(res_start_repr)->val(), expected_start_time);
  EXPECT_EQ(static_cast<IntIR*>(res_stop_repr)->val(), expected_stop_time);
}

TEST_F(CompilerTimeExpressionTest, nested_function) {
  IntIR* constant = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(constant->Init(111, ast));
  IntIR* start = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(start->Init(10, ast));
  FuncIR* stop = graph->MakeNode<FuncIR>().ValueOrDie();
  EXPECT_OK(stop->Init({FuncIR::Opcode::add, "+", "add"}, ASTWalker::kCompileTimeFuncPrefix,
                       std::vector<ExpressionIR*>({MakeConstantAddition(123, 321), constant}),
                       true /* compile_time */, ast));

  RangeIR* range = MakeRange(start, stop);
  RangeArgExpressionRule compiler_expr_rule(compiler_state_.get());

  auto result = compiler_expr_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  IRNode* res_start_repr = range->start_repr();
  IRNode* res_stop_repr = range->stop_repr();
  ASSERT_EQ(res_start_repr->type(), IRNodeType::kInt);
  ASSERT_EQ(res_stop_repr->type(), IRNodeType::kInt);
  EXPECT_EQ(static_cast<IntIR*>(res_start_repr)->val(), 10);
  EXPECT_EQ(static_cast<IntIR*>(res_stop_repr)->val(), 555);
}

TEST_F(CompilerTimeExpressionTest, subtraction_handling) {
  IntIR* constant1 = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(constant1->Init(111, ast));
  IntIR* constant2 = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(constant2->Init(11, ast));
  IntIR* start = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(start->Init(10, ast));
  FuncIR* stop = graph->MakeNode<FuncIR>().ValueOrDie();
  EXPECT_OK(stop->Init({FuncIR::Opcode::sub, "-", "subtract"}, ASTWalker::kCompileTimeFuncPrefix,
                       std::vector<ExpressionIR*>({constant1, constant2}), true /* compile_time */,
                       ast));

  RangeIR* range = MakeRange(start, stop);
  RangeArgExpressionRule compiler_expr_rule(compiler_state_.get());

  auto result = compiler_expr_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  IRNode* res_start_repr = range->start_repr();
  IRNode* res_stop_repr = range->stop_repr();
  ASSERT_EQ(res_start_repr->type(), IRNodeType::kInt);
  ASSERT_EQ(res_stop_repr->type(), IRNodeType::kInt);
  EXPECT_EQ(static_cast<IntIR*>(res_start_repr)->val(), 10);
  EXPECT_EQ(static_cast<IntIR*>(res_stop_repr)->val(), 100);
}

TEST_F(CompilerTimeExpressionTest, multiplication_handling) {
  IntIR* constant1 = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(constant1->Init(3, ast));
  IntIR* constant2 = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(constant2->Init(8, ast));
  IntIR* start = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(start->Init(10, ast));
  FuncIR* stop = graph->MakeNode<FuncIR>().ValueOrDie();
  EXPECT_OK(stop->Init({FuncIR::Opcode::mult, "*", "multiply"}, ASTWalker::kCompileTimeFuncPrefix,
                       std::vector<ExpressionIR*>({constant1, constant2}), true /* compile_time */,
                       ast));

  RangeIR* range = MakeRange(start, stop);
  RangeArgExpressionRule compiler_expr_rule(compiler_state_.get());

  auto result = compiler_expr_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  IRNode* res_start_repr = range->start_repr();
  IRNode* res_stop_repr = range->stop_repr();
  ASSERT_EQ(res_start_repr->type(), IRNodeType::kInt);
  ASSERT_EQ(res_stop_repr->type(), IRNodeType::kInt);
  EXPECT_EQ(static_cast<IntIR*>(res_start_repr)->val(), 10);
  EXPECT_EQ(static_cast<IntIR*>(res_stop_repr)->val(), 24);
}

TEST_F(CompilerTimeExpressionTest, already_completed) {
  IntIR* constant1 = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(constant1->Init(24, ast));
  IntIR* constant2 = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(constant2->Init(8, ast));

  RangeIR* range = MakeRange(constant1, constant2);
  RangeArgExpressionRule compiler_expr_rule(compiler_state_.get());

  auto result = compiler_expr_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());

  IRNode* res_start_repr = range->start_repr();
  IRNode* res_stop_repr = range->stop_repr();
  ASSERT_EQ(res_start_repr->type(), IRNodeType::kInt);
  ASSERT_EQ(res_stop_repr->type(), IRNodeType::kInt);
  EXPECT_EQ(static_cast<IntIR*>(res_start_repr)->val(), 24);
  EXPECT_EQ(static_cast<IntIR*>(res_stop_repr)->val(), 8);
}
class VerifyFilterExpressionTest : public RulesTest {
 protected:
  void SetUp() override {
    RulesTest::SetUp();
    mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
    PL_CHECK_OK(mem_src->SetRelation(cpu_relation));
  }
  FuncIR* MakeFilter() {
    auto constant1 = graph->MakeNode<IntIR>().ValueOrDie();
    EXPECT_OK(constant1->Init(10, ast));

    auto constant2 = graph->MakeNode<IntIR>().ValueOrDie();
    EXPECT_OK(constant2->Init(10, ast));

    filter_func = graph->MakeNode<FuncIR>().ValueOrDie();
    EXPECT_OK(filter_func->Init({FuncIR::Opcode::eq, "==", "equals"}, ASTWalker::kRunTimeFuncPrefix,
                                std::vector<ExpressionIR*>({constant1, constant2}),
                                false /* compile_time */, ast));

    auto filter_func_lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
    EXPECT_OK(filter_func_lambda->Init({}, filter_func, ast));

    FilterIR* filter = graph->MakeNode<FilterIR>().ValueOrDie();
    ArgMap amap({{"fn", filter_func_lambda}});
    EXPECT_OK(filter->Init(mem_src, amap, ast));
    return filter_func;
  }
  MemorySourceIR* mem_src;
  FuncIR* filter_func;
};

TEST_F(VerifyFilterExpressionTest, basic_test) {
  FuncIR* filter_func = MakeFilter();
  filter_func->SetOutputDataType(types::DataType::BOOLEAN);
  VerifyFilterExpressionRule rule(compiler_state_.get());
  auto status_or = rule.Execute(graph.get());
  EXPECT_OK(status_or);
  EXPECT_FALSE(status_or.ValueOrDie());
}

TEST_F(VerifyFilterExpressionTest, wrong_filter_func_type) {
  FuncIR* filter_func = MakeFilter();
  filter_func->SetOutputDataType(types::DataType::INT64);
  VerifyFilterExpressionRule rule(compiler_state_.get());
  auto status_or = rule.Execute(graph.get());
  EXPECT_NOT_OK(status_or);
}

TEST_F(VerifyFilterExpressionTest, filter_func_not_set) {
  FuncIR* filter_func = MakeFilter();
  EXPECT_EQ(filter_func->EvaluatedDataType(), types::DataType::DATA_TYPE_UNKNOWN);
  VerifyFilterExpressionRule rule(compiler_state_.get());
  auto status_or = rule.Execute(graph.get());
  EXPECT_NOT_OK(status_or);
}

class ResolveMetadataTest : public RulesTest {
 protected:
  void SetUp() override {
    RulesTest::SetUp();
    mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
    PL_CHECK_OK(mem_src->SetRelation(cpu_relation));
  }
  FilterIR* MakeFilter(OperatorIR* parent, ColumnIR* filter_value) {
    auto constant1 = graph->MakeNode<StringIR>().ValueOrDie();
    EXPECT_OK(constant1->Init("value", ast));

    FuncIR* filter_func = graph->MakeNode<FuncIR>().ValueOrDie();
    EXPECT_OK(filter_func->Init({FuncIR::Opcode::eq, "==", "equals"}, ASTWalker::kRunTimeFuncPrefix,
                                std::vector<ExpressionIR*>({constant1, filter_value}),
                                false /* compile_time */, ast));

    auto filter_func_lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
    EXPECT_OK(filter_func_lambda->Init({}, filter_func, ast));

    FilterIR* filter = graph->MakeNode<FilterIR>().ValueOrDie();
    ArgMap amap({{"fn", filter_func_lambda}});
    EXPECT_OK(filter->Init(parent, amap, ast));
    return filter;
  }

  MetadataResolverIR* MakeMetadataResolver(OperatorIR* parent) {
    md_resolver = graph->MakeNode<MetadataResolverIR>().ValueOrDie();
    EXPECT_OK(md_resolver->Init(parent, {{}}, ast));
    return md_resolver;
  }
  BlockingAggIR* MakeBlockingAgg(OperatorIR* parent, ColumnIR* by_column, ColumnIR* fn_column) {
    auto agg_func = graph->MakeNode<FuncIR>().ValueOrDie();
    EXPECT_OK(agg_func->Init({FuncIR::Opcode::non_op, "", "mean"}, ASTWalker::kRunTimeFuncPrefix,
                             std::vector<ExpressionIR*>({fn_column}), false /* compile_time */,
                             ast));

    auto agg_func_lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
    EXPECT_OK(agg_func_lambda->Init({}, {{"agg_fn", agg_func}}, ast));

    auto by_func_lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
    EXPECT_OK(by_func_lambda->Init({"group"}, by_column, ast));

    BlockingAggIR* agg = graph->MakeNode<BlockingAggIR>().ValueOrDie();
    ArgMap amap({{"by", by_func_lambda}, {"fn", agg_func_lambda}});
    EXPECT_OK(agg->Init(parent, amap, ast));
    return agg;
  }
  MemorySourceIR* mem_src;
  MetadataResolverIR* md_resolver;
};

TEST_F(ResolveMetadataTest, create_metadata_resolver) {
  // a MetadataIR unresovled creates a new metadata node.
  MetadataIR* metadata = MakeMetadataIR("service_name", mem_src);
  EXPECT_FALSE(metadata->HasMetadataResolver());
  FilterIR* filter = MakeFilter(mem_src, metadata);
  EXPECT_EQ(filter->parents()[0]->id(), mem_src->id());

  ResolveMetadataRule rule(compiler_state_.get(), md_handler.get());
  rule.Execute(graph.get());
  EXPECT_NE(filter->parents()[0]->id(), mem_src->id());
  ASSERT_EQ(filter->parents()[0]->type(), IRNodeType::kMetadataResolver);
  auto md_resolver = static_cast<MetadataResolverIR*>(filter->parents()[0]);
  EXPECT_TRUE(md_resolver->HasMetadataColumn("service_name"));
  EXPECT_TRUE(metadata->HasMetadataResolver());
  EXPECT_EQ(metadata->referenced_op(), md_resolver);
  EXPECT_EQ(metadata->resolver(), md_resolver);
}

TEST_F(ResolveMetadataTest, update_metadata_resolver) {
  // a MetadataIR unresovled updates an existing metadata resolver, that has one entry
  MetadataResolverIR* og_resolver = MakeMetadataResolver(mem_src);
  // prepopulate the og_resolver.
  auto property = md_handler->GetProperty("pod_name").ValueOrDie();
  ASSERT_OK(og_resolver->AddMetadata(property));
  EXPECT_TRUE(og_resolver->HasMetadataColumn("pod_name"));
  // Should not have service_name yet.
  EXPECT_FALSE(og_resolver->HasMetadataColumn("service_name"));

  MetadataIR* metadata = MakeMetadataIR("service_name", og_resolver);
  EXPECT_FALSE(metadata->HasMetadataResolver());
  FilterIR* filter = MakeFilter(og_resolver, metadata);
  EXPECT_EQ(filter->parents()[0]->id(), og_resolver->id());

  ResolveMetadataRule rule(compiler_state_.get(), md_handler.get());
  rule.Execute(graph.get());
  // no changes.
  EXPECT_EQ(filter->parents()[0]->id(), og_resolver->id());

  auto md_resolver = static_cast<MetadataResolverIR*>(filter->parents()[0]);
  EXPECT_EQ(md_resolver, og_resolver);
  EXPECT_TRUE(md_resolver->HasMetadataColumn("service_name"));
  EXPECT_TRUE(md_resolver->HasMetadataColumn("pod_name"));
  EXPECT_TRUE(metadata->HasMetadataResolver());
  EXPECT_EQ(metadata->referenced_op(), md_resolver);
  EXPECT_EQ(metadata->resolver(), og_resolver);
}

TEST_F(ResolveMetadataTest, multiple_mds_in_one_op) {
  // Two metadata callsin one operation.
  MetadataIR* metadata1 = MakeMetadataIR("service_name", mem_src);
  MetadataIR* metadata2 = MakeMetadataIR("pod_name", mem_src);
  EXPECT_FALSE(metadata1->HasMetadataResolver());
  EXPECT_FALSE(metadata2->HasMetadataResolver());
  BlockingAggIR* agg = MakeBlockingAgg(mem_src, metadata1, metadata2);
  EXPECT_EQ(agg->parents()[0]->id(), mem_src->id());

  ResolveMetadataRule rule(compiler_state_.get(), md_handler.get());
  rule.Execute(graph.get());
  EXPECT_NE(agg->parents()[0]->id(), mem_src->id());
  ASSERT_EQ(agg->parents()[0]->type(), IRNodeType::kMetadataResolver)
      << agg->parents()[0]->type_string();
  // no layers of md.
  ASSERT_EQ(agg->parents()[0]->parents()[0]->type(), IRNodeType::kMemorySource);
  auto md_resolver = static_cast<MetadataResolverIR*>(agg->parents()[0]);
  EXPECT_TRUE(md_resolver->HasMetadataColumn("service_name"));
  EXPECT_TRUE(md_resolver->HasMetadataColumn("pod_name"));
  EXPECT_TRUE(metadata1->HasMetadataResolver());
  EXPECT_EQ(metadata1->resolver(), md_resolver);

  EXPECT_TRUE(metadata2->HasMetadataResolver());
  EXPECT_EQ(metadata2->resolver(), md_resolver);

  EXPECT_EQ(metadata1->referenced_op(), md_resolver)
      << metadata1->referenced_op()->type_string() << " vs " << md_resolver->type_string();
  EXPECT_EQ(metadata2->referenced_op(), md_resolver)
      << metadata2->referenced_op()->type_string() << " vs " << md_resolver->type_string();
}

TEST_F(ResolveMetadataTest, no_change_metadata_resolver) {
  // MetadataIR where the metadataresolver node already has an entry lined up properly[]
  // a MetadataIR unresovled updates an existing metadata resolver, that has one entry
  MetadataResolverIR* og_resolver = MakeMetadataResolver(mem_src);
  auto property = md_handler->GetProperty("pod_name").ValueOrDie();
  ASSERT_OK(og_resolver->AddMetadata(property));
  EXPECT_TRUE(og_resolver->HasMetadataColumn("pod_name"));

  MetadataIR* metadata = MakeMetadataIR("pod_name", og_resolver);
  EXPECT_FALSE(metadata->HasMetadataResolver());
  FilterIR* filter = MakeFilter(og_resolver, metadata);
  EXPECT_EQ(filter->parents()[0]->id(), og_resolver->id());

  ResolveMetadataRule rule(compiler_state_.get(), md_handler.get());
  rule.Execute(graph.get());
  // no changes.
  EXPECT_EQ(filter->parents()[0]->id(), og_resolver->id());

  auto md_resolver = static_cast<MetadataResolverIR*>(filter->parents()[0]);
  EXPECT_EQ(md_resolver, og_resolver);
  EXPECT_TRUE(md_resolver->HasMetadataColumn("pod_name"));
  EXPECT_TRUE(metadata->HasMetadataResolver());
  EXPECT_EQ(metadata->referenced_op(), og_resolver)
      << metadata->referenced_op()->type_string() << " vs " << og_resolver->type_string();
}

class FormatMetadataTest : public RulesTest {
 protected:
  void SetUp() override {
    RulesTest::SetUp();
    mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
    PL_CHECK_OK(mem_src->SetRelation(cpu_relation));
    md_resolver = graph->MakeNode<MetadataResolverIR>().ValueOrDie();
    PL_CHECK_OK(md_resolver->Init(mem_src, {{}}, ast));
  }

  MetadataIR* MakeMetadataIR(const std::string& name, OperatorIR* parent_op) {
    auto metadata = graph->MakeNode<MetadataIR>().ValueOrDie();
    PL_CHECK_OK(metadata->Init(name, parent_op, ast));
    MetadataProperty* property = md_handler->GetProperty(name).ValueOrDie();
    PL_CHECK_OK(metadata->ResolveMetadataColumn(md_resolver, property));
    return metadata;
  }

  MemorySourceIR* mem_src;
  MetadataResolverIR* md_resolver;
};

TEST_F(FormatMetadataTest, string_matches_format) {
  // equiv to `r.attr.pod_name == pod-xyzx`
  FuncIR* equals_func =
      MakeEqualsFunc(MakeMetadataIR("pod_name", mem_src), MakeString("namespace/pod-xyzx"));
  EXPECT_EQ(equals_func->args()[0]->type(), IRNodeType::kMetadata);
  EXPECT_EQ(equals_func->args()[1]->type(), IRNodeType::kString);

  MetadataFunctionFormatRule rule(compiler_state_.get());
  auto status = rule.Execute(graph.get());

  EXPECT_OK(status);
  EXPECT_TRUE(status.ValueOrDie());

  ASSERT_EQ(equals_func->args().size(), 2UL);
  EXPECT_EQ(equals_func->args()[0]->type(), IRNodeType::kMetadata);
  EXPECT_EQ(equals_func->args()[1]->type(), IRNodeType::kMetadataLiteral);

  // Rule should do nothing.
  status = rule.Execute(graph.get());

  EXPECT_OK(status);
  EXPECT_FALSE(status.ValueOrDie());
  ASSERT_EQ(equals_func->args().size(), 2UL);
  EXPECT_EQ(equals_func->args()[0]->type(), IRNodeType::kMetadata);
  EXPECT_EQ(equals_func->args()[1]->type(), IRNodeType::kMetadataLiteral);
}

TEST_F(FormatMetadataTest, bad_format) {
  FuncIR* equals_func = MakeEqualsFunc(MakeMetadataIR("pod_name", mem_src), MakeString("pod-xyzx"));
  EXPECT_EQ(equals_func->args()[0]->type(), IRNodeType::kMetadata);
  EXPECT_EQ(equals_func->args()[1]->type(), IRNodeType::kString);

  MetadataFunctionFormatRule rule(compiler_state_.get());
  auto status = rule.Execute(graph.get());

  EXPECT_NOT_OK(status);
  EXPECT_THAT(status.status(),
              HasCompilerError("String not formatted properly for metadata operation. "
                               "Expected String with format <namespace>/<name>."));
}

TEST_F(FormatMetadataTest, equals_fails_when_not_string) {
  // equiv to `r.attr.pod_name == 10`
  FuncIR* equals_func = MakeEqualsFunc(MakeMetadataIR("pod_name", mem_src), MakeInt(10));
  MetadataFunctionFormatRule rule(compiler_state_.get());
  EXPECT_EQ(equals_func->args()[0]->type(), IRNodeType::kMetadata);
  EXPECT_EQ(equals_func->args()[1]->type(), IRNodeType::kInt);
  auto status = rule.Execute(graph.get());

  EXPECT_NOT_OK(status);
  EXPECT_THAT(status.status(), HasCompilerError("Function \'pl.equals\' with metadata arg in "
                                                "conjunction with \'[Int]\' is not supported."));
}

TEST_F(FormatMetadataTest, only_equal_supported) {
  // equiv to `r.attr.pod_name == 10`
  FuncIR* add_func = MakeAddFunc(MakeMetadataIR("pod_name", mem_src), MakeString("pod-xyzx"));
  MetadataFunctionFormatRule rule(compiler_state_.get());
  EXPECT_EQ(add_func->args()[0]->type(), IRNodeType::kMetadata);
  EXPECT_EQ(add_func->args()[1]->type(), IRNodeType::kString);
  auto status = rule.Execute(graph.get());

  EXPECT_NOT_OK(status);
  EXPECT_THAT(status.status(), HasCompilerError("Function \'pl.add\' with metadata arg in "
                                                "conjunction with \'[String]\' is not supported."));
}

class CheckRelationRule : public RulesTest {
 protected:
  void SetUp() override {
    RulesTest::SetUp();
    mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
    PL_CHECK_OK(mem_src->SetRelation(cpu_relation));
  }

  MapIR* MakeMap(OperatorIR* parent, std::string column_name) {
    auto map_func = graph->MakeNode<FuncIR>().ValueOrDie();
    EXPECT_OK(map_func->Init({FuncIR::Opcode::add, "+", "add"}, ASTWalker::kRunTimeFuncPrefix,
                             std::vector<ExpressionIR*>({MakeInt(10), MakeInt(12)}),
                             false /* compile_time */, ast));

    auto map_func_lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
    EXPECT_OK(map_func_lambda->Init({}, {{column_name, map_func}}, ast));

    MapIR* map = graph->MakeNode<MapIR>().ValueOrDie();
    ArgMap amap({{"fn", map_func_lambda}});
    EXPECT_OK(map->Init(parent, amap, ast));
    return map;
  }

  MapIR* MakeMap(OperatorIR* parent) { return MakeMap(parent, "map_fn"); }

  table_store::schema::Relation ViolatingRelation() {
    auto relation = mem_src->relation();
    relation.AddColumn(types::DataType::STRING,
                       absl::Substitute("$0_pod_name", MetadataProperty::kMetadataColumnPrefix));
    return relation;
  }
  table_store::schema::Relation PassingRelation() { return mem_src->relation(); }

  MemorySourceIR* mem_src;
};

// Should not have any violations or exceptions in this case.
TEST_F(CheckRelationRule, properly_formatted_no_problems) {
  FilterIR* filter = MakeFilter(mem_src);
  EXPECT_OK(filter->SetRelation(PassingRelation()));
  MapIR* map = MakeMap(filter);
  EXPECT_OK(map->SetRelation(PassingRelation()));

  CheckMetadataColumnNamingRule rule(compiler_state_.get());

  auto status = rule.Execute(graph.get());
  ASSERT_OK(status);
  // Should not change anything.
  EXPECT_FALSE(status.ValueOrDie());
}

// Metadata would be violated if it wasn't evaluated correctly, but it is the only node that has an
// exception.
TEST_F(CheckRelationRule, skip_metadata_resolver) {
  MetadataResolverIR* md_resolver = MakeMetadataResolver(mem_src);
  EXPECT_OK(md_resolver->SetRelation(ViolatingRelation()));
  MapIR* map = MakeMap(md_resolver);
  EXPECT_OK(map->SetRelation(PassingRelation()));

  CheckMetadataColumnNamingRule rule(compiler_state_.get());

  auto status = rule.Execute(graph.get());
  ASSERT_OK(status);
  // Should not change anything.
  EXPECT_FALSE(status.ValueOrDie());
}
// Should find an issue with the map function.
TEST_F(CheckRelationRule, find_map_issue) {
  std::string column_name = absl::Substitute("$0service", MetadataProperty::kMetadataColumnPrefix);
  MakeMap(mem_src, column_name);

  CheckMetadataColumnNamingRule rule(compiler_state_.get());

  auto status = rule.Execute(graph.get());
  ASSERT_NOT_OK(status) << "Expected rule execution to fail.";
  EXPECT_THAT(
      status.status(),
      HasCompilerError(
          "Column name '$1' violates naming rules. The '$0' prefix is reserved for internal use.",
          MetadataProperty::kMetadataColumnPrefix, column_name));
}

class MetadataResolverConversionTest : public RulesTest {
 protected:
  void SetUp() override {
    RulesTest::SetUp();
    mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
    PL_CHECK_OK(mem_src->SetRelation(cpu_relation));
  }

  MapIR* MakeMap(OperatorIR* parent) {
    auto agg_func = graph->MakeNode<FuncIR>().ValueOrDie();
    EXPECT_OK(agg_func->Init({FuncIR::Opcode::add, "+", "add"}, ASTWalker::kRunTimeFuncPrefix,
                             std::vector<ExpressionIR*>({MakeInt(10), MakeInt(12)}),
                             false /* compile_time */, ast));

    auto agg_func_lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
    EXPECT_OK(agg_func_lambda->Init({}, {{"agg_fn", agg_func}}, ast));

    MapIR* agg = graph->MakeNode<MapIR>().ValueOrDie();
    ArgMap amap({{"fn", agg_func_lambda}});
    EXPECT_OK(agg->Init(parent, amap, ast));
    return agg;
  }

  MemorySourceIR* mem_src;
};

// Test to make sure that joining of metadata works with upid, most common case.
TEST_F(MetadataResolverConversionTest, upid_conversion) {
  auto relation = table_store::schema::Relation(cpu_relation);
  MetadataType conversion_column = MetadataType::UPID;
  std::string conversion_column_str = MetadataProperty::GetMetadataString(conversion_column);
  relation.AddColumn(types::DataType::UINT128, conversion_column_str);
  EXPECT_OK(mem_src->SetRelation(relation));
  NameMetadataProperty property(MetadataType::POD_NAME, {MetadataType::UPID});
  MetadataResolverIR* md_resolver = MakeMetadataResolver(mem_src);
  ASSERT_OK(md_resolver->AddMetadata(&property));

  auto md_relation = table_store::schema::Relation(relation);
  md_relation.AddColumn(property.column_type(), property.GetColumnRepr());
  EXPECT_OK(md_resolver->SetRelation(md_relation));

  MetadataIR* metadata_ir = MakeMetadataIR("pod_name", mem_src);
  ASSERT_OK(metadata_ir->ResolveMetadataColumn(md_resolver, &property));
  FilterIR* filter = MakeFilter(md_resolver, metadata_ir);

  MetadataResolverConversionRule rule(compiler_state_.get());
  auto status = rule.Execute(graph.get());
  ASSERT_OK(status);
  EXPECT_TRUE(status.ValueOrDie());

  OperatorIR* new_op = filter->parents()[0];
  EXPECT_NE(new_op, md_resolver);
  ASSERT_EQ(new_op->type(), IRNodeType::kMap) << "Expected Map, got " << new_op->type_string();
  EXPECT_EQ(new_op->parents()[0], mem_src);

  // Check to make sure there are no edges between the mem_src and md_resolver.
  // Check to make sure there are no edges between the filter and md_resolver.
  EXPECT_FALSE(graph->dag().HasEdge(mem_src->id(), md_resolver->id()));
  EXPECT_FALSE(graph->dag().HasEdge(md_resolver->id(), filter->id()));

  // Check to make sure new edges are referenced.
  EXPECT_TRUE(graph->dag().HasEdge(mem_src->id(), new_op->id()));
  EXPECT_TRUE(graph->dag().HasEdge(new_op->id(), filter->id()));

  MapIR* md_mapper = static_cast<MapIR*>(new_op);

  ColExpressionVector vec = md_mapper->col_exprs();
  ASSERT_EQ(relation.NumColumns() + md_resolver->metadata_columns().size(), vec.size());
  // Check to see that all of the parent columns are there
  int64_t cur_idx = 0;
  for (const std::string& parent_col_name : relation.col_names()) {
    ColumnExpression expr_pair = vec[cur_idx];
    EXPECT_EQ(expr_pair.name, parent_col_name);
    ASSERT_EQ(expr_pair.node->type(), IRNodeType::kColumn);
    ColumnIR* col_ir = static_cast<ColumnIR*>(expr_pair.node);
    EXPECT_EQ(col_ir->col_name(), parent_col_name);
    cur_idx += 1;
  }

  // Check to see that the metadata columns have the correct format.
  for (const auto& md_iter : md_resolver->metadata_columns()) {
    std::string md_col_name = md_iter.first;
    ColumnExpression expr_pair = vec[cur_idx];
    EXPECT_EQ(expr_pair.name, MetadataProperty::FormatMetadataColumn(md_col_name));
    ASSERT_EQ(expr_pair.node->type(), IRNodeType::kFunc) << absl::Substitute(
        "Expected function for idx $0, got $1.", cur_idx, expr_pair.node->type_string());
    FuncIR* func = static_cast<FuncIR*>(expr_pair.node);
    std::string udf_name = absl::Substitute(
        "pl.$0_to_$1", MetadataProperty::GetMetadataString(conversion_column), md_col_name);
    EXPECT_EQ(udf_name, func->func_name());
    ASSERT_EQ(func->args().size(), 1) << absl::Substitute("for idx $0.", cur_idx);
    ExpressionIR* func_arg = func->args()[0];
    ASSERT_EQ(func_arg->type(), IRNodeType::kColumn) << absl::Substitute(
        "Expected column for idx $0, got $1.", cur_idx, func_arg->type_string());
    ColumnIR* col_ir = static_cast<ColumnIR*>(func_arg);
    EXPECT_EQ(col_ir->col_name(), conversion_column_str);
    cur_idx += 1;
  }
}

TEST_F(MetadataResolverConversionTest, alternative_column) {
  auto relation = table_store::schema::Relation(cpu_relation);
  MetadataType conversion_column = MetadataType::POD_ID;
  std::string conversion_column_str = MetadataProperty::FormatMetadataColumn(conversion_column);
  relation.AddColumn(types::DataType::STRING, conversion_column_str);
  EXPECT_OK(mem_src->SetRelation(relation));
  NameMetadataProperty property(MetadataType::POD_NAME, {MetadataType::UPID, MetadataType::POD_ID});
  MetadataResolverIR* md_resolver = MakeMetadataResolver(mem_src);
  ASSERT_OK(md_resolver->AddMetadata(&property));

  auto md_relation = table_store::schema::Relation(relation);
  md_relation.AddColumn(property.column_type(), property.GetColumnRepr());
  EXPECT_OK(md_resolver->SetRelation(md_relation));

  MetadataIR* metadata_ir = MakeMetadataIR("pod_name", mem_src);
  ASSERT_OK(metadata_ir->ResolveMetadataColumn(md_resolver, &property));
  FilterIR* filter = MakeFilter(md_resolver, metadata_ir);

  MetadataResolverConversionRule rule(compiler_state_.get());
  auto status = rule.Execute(graph.get());
  ASSERT_OK(status);
  EXPECT_TRUE(status.ValueOrDie());

  OperatorIR* new_op = filter->parents()[0];
  EXPECT_NE(new_op, md_resolver);
  ASSERT_EQ(new_op->type(), IRNodeType::kMap) << "Expected Map, got " << new_op->type_string();

  MapIR* md_mapper = static_cast<MapIR*>(new_op);

  ColExpressionVector vec = md_mapper->col_exprs();

  int64_t cur_idx = static_cast<int64_t>(relation.col_names().size());

  // Check to see that the metadata columns have the correct format.
  for (const auto& md_iter : md_resolver->metadata_columns()) {
    std::string md_col_name = md_iter.first;
    ColumnExpression expr_pair = vec[cur_idx];
    EXPECT_EQ(expr_pair.name, MetadataProperty::FormatMetadataColumn(md_col_name));
    ASSERT_EQ(expr_pair.node->type(), IRNodeType::kFunc) << absl::Substitute(
        "Expected function for idx $0, got $1.", cur_idx, expr_pair.node->type_string());
    FuncIR* func = static_cast<FuncIR*>(expr_pair.node);
    std::string udf_name = absl::Substitute(
        "pl.$0_to_$1", MetadataProperty::GetMetadataString(conversion_column), md_col_name);
    EXPECT_EQ(udf_name, func->func_name());
    ASSERT_EQ(func->args().size(), 1) << absl::Substitute("for idx $0.", cur_idx);
    ExpressionIR* func_arg = func->args()[0];
    ASSERT_EQ(func_arg->type(), IRNodeType::kColumn) << absl::Substitute(
        "Expected column for idx $0, got $1.", cur_idx, func_arg->type_string());
    ColumnIR* col_ir = static_cast<ColumnIR*>(func_arg);
    EXPECT_EQ(col_ir->col_name(), conversion_column_str);
    cur_idx += 1;
  }
}

TEST_F(MetadataResolverConversionTest, missing_conversion_column) {
  auto relation = table_store::schema::Relation(cpu_relation);
  EXPECT_OK(mem_src->SetRelation(relation));
  NameMetadataProperty property(MetadataType::POD_NAME, {MetadataType::UPID});
  MetadataResolverIR* md_resolver = MakeMetadataResolver(mem_src);
  ASSERT_OK(md_resolver->AddMetadata(&property));

  auto md_relation = table_store::schema::Relation(relation);
  md_relation.AddColumn(property.column_type(), property.GetColumnRepr());
  EXPECT_OK(md_resolver->SetRelation(md_relation));

  MetadataIR* metadata_ir = MakeMetadataIR("pod_name", mem_src);
  ASSERT_OK(metadata_ir->ResolveMetadataColumn(md_resolver, &property));
  MakeFilter(md_resolver, metadata_ir);

  MetadataResolverConversionRule rule(compiler_state_.get());
  auto status = rule.Execute(graph.get());
  EXPECT_NOT_OK(status);
  VLOG(1) << status.ToString();
  EXPECT_THAT(status.status(),
              HasCompilerError(
                  "Can\'t resolve metadata because of lack of converting columns in the parent. "
                  "Need one of "
                  "[upid]. Parent relation has columns [count,cpu0,cpu1,cpu2] available."));
}

// When the parent relation has multiple columns that can be converted into
// columns, the compiler makes a choice on which column to replace.
TEST_F(MetadataResolverConversionTest, multiple_conversion_columns) {
  auto relation = table_store::schema::Relation(cpu_relation);
  MetadataType conversion_column1 = MetadataType::UPID;
  std::string conversion_column1_str = MetadataProperty::FormatMetadataColumn(conversion_column1);
  MetadataType conversion_column2 = MetadataType::POD_ID;
  relation.AddColumn(types::DataType::UINT128,
                     MetadataProperty::FormatMetadataColumn(conversion_column1));
  relation.AddColumn(types::DataType::STRING,
                     MetadataProperty::FormatMetadataColumn(conversion_column2));
  EXPECT_OK(mem_src->SetRelation(relation));
  NameMetadataProperty property(MetadataType::POD_NAME, {conversion_column1, conversion_column2});
  MetadataResolverIR* md_resolver = MakeMetadataResolver(mem_src);
  ASSERT_OK(md_resolver->AddMetadata(&property));

  auto md_relation = table_store::schema::Relation(relation);
  md_relation.AddColumn(property.column_type(), property.GetColumnRepr());
  EXPECT_OK(md_resolver->SetRelation(md_relation));

  MetadataIR* metadata_ir = MakeMetadataIR("pod_name", mem_src);
  ASSERT_OK(metadata_ir->ResolveMetadataColumn(md_resolver, &property));
  FilterIR* filter = MakeFilter(md_resolver, metadata_ir);

  MetadataResolverConversionRule rule(compiler_state_.get());
  auto status = rule.Execute(graph.get());
  ASSERT_OK(status);
  EXPECT_TRUE(status.ValueOrDie());

  OperatorIR* new_op = filter->parents()[0];
  EXPECT_NE(new_op, md_resolver);
  ASSERT_EQ(new_op->type(), IRNodeType::kMap) << "Expected Map, got " << new_op->type_string();

  MapIR* md_mapper = static_cast<MapIR*>(new_op);
  ColExpressionVector vec = md_mapper->col_exprs();

  int64_t cur_idx = static_cast<int64_t>(relation.col_names().size());

  // Check to see that the metadata columns have the correct format.
  for (const auto& md_iter : md_resolver->metadata_columns()) {
    std::string md_col_name = md_iter.first;
    ColumnExpression expr_pair = vec[cur_idx];
    EXPECT_EQ(expr_pair.name, MetadataProperty::FormatMetadataColumn(md_col_name));
    ASSERT_EQ(expr_pair.node->type(), IRNodeType::kFunc) << absl::Substitute(
        "Expected function for idx $0, got $1.", cur_idx, expr_pair.node->type_string());
    FuncIR* func = static_cast<FuncIR*>(expr_pair.node);
    std::string udf_name =
        absl::Substitute("pl.$0_to_$1", MetadataProperty::kUniquePIDColumn, md_col_name);
    EXPECT_EQ(udf_name, func->func_name());
    ASSERT_EQ(func->args().size(), 1) << absl::Substitute("for idx $0.", cur_idx);
    ExpressionIR* func_arg = func->args()[0];
    ASSERT_EQ(func_arg->type(), IRNodeType::kColumn) << absl::Substitute(
        "Expected column for idx $0, got $1.", cur_idx, func_arg->type_string());
    ColumnIR* col_ir = static_cast<ColumnIR*>(func_arg);
    EXPECT_EQ(col_ir->col_name(), conversion_column1_str);
    cur_idx += 1;
  }
}

// Make sure that the mapping works for multiple columns.
TEST_F(MetadataResolverConversionTest, multiple_metadata_columns) {
  auto relation = table_store::schema::Relation(cpu_relation);
  MetadataType conversion_column = MetadataType::UPID;
  std::string conversion_column_str = MetadataProperty::GetMetadataString(conversion_column);
  relation.AddColumn(types::DataType::UINT128, conversion_column_str);
  EXPECT_OK(mem_src->SetRelation(relation));
  NameMetadataProperty property1(MetadataType::POD_NAME, {conversion_column});
  NameMetadataProperty property2(MetadataType::SERVICE_NAME, {conversion_column});
  MetadataResolverIR* md_resolver = MakeMetadataResolver(mem_src);
  ASSERT_OK(md_resolver->AddMetadata(&property1));
  ASSERT_OK(md_resolver->AddMetadata(&property2));

  auto md_relation = table_store::schema::Relation(relation);
  md_relation.AddColumn(property1.column_type(), property1.GetColumnRepr());
  md_relation.AddColumn(property2.column_type(), property2.GetColumnRepr());
  EXPECT_OK(md_resolver->SetRelation(md_relation));

  FilterIR* filter = MakeFilter(md_resolver);

  MetadataResolverConversionRule rule(compiler_state_.get());
  auto status = rule.Execute(graph.get());
  ASSERT_OK(status);
  EXPECT_TRUE(status.ValueOrDie());

  OperatorIR* new_op = filter->parents()[0];
  EXPECT_NE(new_op, md_resolver);
  ASSERT_EQ(new_op->type(), IRNodeType::kMap) << "Expected Map, got " << new_op->type_string();

  MapIR* md_mapper = static_cast<MapIR*>(new_op);
  ColExpressionVector vec = md_mapper->col_exprs();

  int64_t cur_idx = static_cast<int64_t>(relation.col_names().size());

  // Check to see that the metadata columns have the correct format.
  for (const auto& md_iter : md_resolver->metadata_columns()) {
    std::string md_col_name = md_iter.first;
    ColumnExpression expr_pair = vec[cur_idx];
    EXPECT_EQ(expr_pair.name, MetadataProperty::FormatMetadataColumn(md_col_name));
    ASSERT_EQ(expr_pair.node->type(), IRNodeType::kFunc) << absl::Substitute(
        "Expected function for idx $0, got $1.", cur_idx, expr_pair.node->type_string());
    FuncIR* func = static_cast<FuncIR*>(expr_pair.node);
    std::string udf_name =
        absl::Substitute("pl.$0_to_$1", MetadataProperty::kUniquePIDColumn, md_col_name);
    EXPECT_EQ(udf_name, func->func_name());
    ASSERT_EQ(func->args().size(), 1) << absl::Substitute("for idx $0.", cur_idx);
    ExpressionIR* func_arg = func->args()[0];
    ASSERT_EQ(func_arg->type(), IRNodeType::kColumn) << absl::Substitute(
        "Expected column for idx $0, got $1.", cur_idx, func_arg->type_string());
    ColumnIR* col_ir = static_cast<ColumnIR*>(func_arg);
    EXPECT_EQ(col_ir->col_name(), conversion_column_str);
    cur_idx += 1;
  }
}

// Test to make sure that the rule gets rid of extra metadata resolvers if the parent
// already has the appropriate column.
TEST_F(MetadataResolverConversionTest, remove_extra_resolver) {
  auto relation = table_store::schema::Relation(cpu_relation);
  MetadataType conversion_column = MetadataType::UPID;
  std::string conversion_column_str = MetadataProperty::GetMetadataString(conversion_column);
  relation.AddColumn(types::DataType::UINT128, conversion_column_str);
  EXPECT_OK(mem_src->SetRelation(relation));
  NameMetadataProperty property(MetadataType::POD_NAME, {conversion_column});

  MetadataResolverIR* md_resolver1 = MakeMetadataResolver(mem_src);
  ASSERT_OK(md_resolver1->AddMetadata(&property));

  auto md_relation = table_store::schema::Relation(relation);
  md_relation.AddColumn(property.column_type(), property.GetColumnRepr());
  EXPECT_OK(md_resolver1->SetRelation(md_relation));

  MetadataIR* metadata_ir1 = MakeMetadataIR("pod_name", mem_src);
  ASSERT_OK(metadata_ir1->ResolveMetadataColumn(md_resolver1, &property));

  FilterIR* filter = MakeFilter(md_resolver1, metadata_ir1);
  // Filter just copies columns, so relation is the same.
  EXPECT_OK(filter->SetRelation(md_relation));

  MetadataResolverIR* md_resolver2 = MakeMetadataResolver(filter);
  ASSERT_OK(md_resolver2->AddMetadata(&property));
  // Filter just copies columns, so relation is the same.
  EXPECT_OK(md_resolver2->SetRelation(md_relation));
  MetadataIR* metadata_ir2 = MakeMetadataIR("pod_name", mem_src);
  ASSERT_OK(metadata_ir2->ResolveMetadataColumn(md_resolver2, &property));

  BlockingAggIR* agg = MakeBlockingAgg(md_resolver2, metadata_ir2, MakeColumn("cpu0", filter));
  // Filter just copies columns, so relation is the same.
  EXPECT_OK(agg->SetRelation(relation));

  MetadataResolverConversionRule rule(compiler_state_.get());
  auto status = rule.Execute(graph.get());
  ASSERT_OK(status);
  EXPECT_TRUE(status.ValueOrDie());

  EXPECT_EQ(agg->parents()[0], filter);
  EXPECT_EQ(graph->dag().DependenciesOf(md_resolver2->id()).size(), 0UL);
}

TEST_F(RulesTest, remove_range) {
  // Create nodes.
  int64_t start_time_ns = 2;
  int64_t stop_time_ns = 4;

  MemorySourceIR* mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  RangeIR* range = graph->MakeNode<RangeIR>().ValueOrDie();
  IntIR* start_time = MakeInt(start_time_ns);
  IntIR* stop_time = MakeInt(stop_time_ns);
  MemorySinkIR* sink = graph->MakeNode<MemorySinkIR>().ValueOrDie();
  StringIR* sink_name = MakeString("sink");

  EXPECT_OK(mem_src->SetRelation(cpu_relation));
  EXPECT_OK(range->Init(mem_src, start_time, stop_time, ast));

  ArgMap amap;
  amap["name"] = sink_name;
  EXPECT_OK(sink->Init(range, amap, ast));
  EXPECT_FALSE(mem_src->IsTimeSet());
  EXPECT_EQ(std::vector<int64_t>({0, 1, 2, 3, 4, 5}), graph->dag().TopologicalSort());

  // Apply the rule.
  MergeRangeOperatorRule rule(compiler_state_.get());
  auto status = rule.Execute(graph.get());
  ASSERT_OK(status);
  EXPECT_TRUE(status.ValueOrDie());

  // checks to make sure that all the edges related to range are removed.
  EXPECT_EQ(std::vector<int64_t>({0, 4, 5}), graph->dag().TopologicalSort());
  EXPECT_TRUE(mem_src->IsTimeSet());
  EXPECT_EQ(stop_time_ns - start_time_ns, mem_src->time_stop_ns() - mem_src->time_start_ns());
  EXPECT_EQ(stop_time_ns, mem_src->time_stop_ns());
  EXPECT_EQ(start_time_ns, mem_src->time_start_ns());

  EXPECT_FALSE(graph->dag().HasNode(range->id()));
  EXPECT_FALSE(graph->dag().HasNode(start_time->id()));
  EXPECT_FALSE(graph->dag().HasNode(start_time->id()));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
