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

#include <gtest/gtest.h>
#include <memory>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/planner/objects/dataframe.h"
#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/metadata_object.h"
#include "src/carnot/planner/objects/none_object.h"
#include "src/carnot/planner/objects/pixie_module.h"
#include "src/carnot/planner/objects/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

StatusOr<QLObjectPtr> UDFHandler(IR* graph, const std::string& name, const pypa::AstPtr& ast,
                                 const ParsedArgs&, ASTVisitor* visitor) {
  PX_ASSIGN_OR_RETURN(FuncIR * node,
                      graph->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", name},
                                                std::vector<ExpressionIR*>{}));
  return ExprObject::Create(node, visitor);
}

class DataframeTest : public QLObjectTest {
 protected:
  void SetUp() override {
    QLObjectTest::SetUp();
    src = MakeMemSource();
    ASSERT_OK_AND_ASSIGN(df, Dataframe::Create(compiler_state.get(), src, ast_visitor.get()));
    var_table = VarTable::Create();
    var_table->Add("df", df);
    var_table->Add("mean", FuncObject::Create(
                               "mean", {}, {},
                               /* has_variable_len_args */ true,
                               /* has_variable_len_kwargs */ false,
                               std::bind(&UDFHandler, graph.get(), "mean", std::placeholders::_1,
                                         std::placeholders::_2, std::placeholders::_3),
                               ast_visitor.get())
                               .ConsumeValueOrDie());
  }

  std::shared_ptr<VarTable> var_table;
  std::shared_ptr<Dataframe> df;
  MemorySourceIR* src;
};

TEST_F(DataframeTest, Merge_ListKeys) {
  MemorySourceIR* src2 = MakeMemSource();
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> df2,
                       Dataframe::Create(compiler_state.get(), src2, ast_visitor.get()));
  var_table->Add("df2", df2);

  std::string script = R"pxl(join = df.merge(
    df2,
    how='inner',
    left_on=['a','b'],
    right_on=['b','c'],
    suffixes=['_x', '_y'],
  ))pxl";
  ASSERT_OK(ParseScript(var_table, script));
  auto var = var_table->Lookup("join");
  ASSERT_EQ(var->type_descriptor().type(), QLObjectType::kDataframe);
  auto join_df = static_cast<Dataframe*>(var.get());

  // Check to make sure that the operator is a join.
  OperatorIR* op = join_df->op();
  ASSERT_MATCH(op, Join());

  JoinIR* join = static_cast<JoinIR*>(op);
  // Verify that the operator does what we expect it to.
  EXPECT_THAT(join->parents(), ElementsAre(src, src2));
  EXPECT_EQ(join->join_type(), JoinIR::JoinType::kInner);

  EXPECT_EQ(2, join->left_on_columns().size());
  EXPECT_MATCH(join->left_on_columns()[0], ColumnNode("a", 0));
  EXPECT_MATCH(join->left_on_columns()[1], ColumnNode("b", 0));

  EXPECT_EQ(2, join->right_on_columns().size());
  EXPECT_MATCH(join->right_on_columns()[0], ColumnNode("b", 1));
  EXPECT_MATCH(join->right_on_columns()[1], ColumnNode("c", 1));

  EXPECT_THAT(join->suffix_strs(), ElementsAre("_x", "_y"));
}

TEST_F(DataframeTest, Merge_NonListKeys) {
  MemorySourceIR* src2 = MakeMemSource();
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> df2,
                       Dataframe::Create(compiler_state.get(), src2, ast_visitor.get()));
  var_table->Add("df2", df2);

  std::string script = R"pxl(join = df.merge(
    df2,
    how='inner',
    left_on='a',
    right_on='b',
    suffixes=['_x', '_y'],
  ))pxl";
  ASSERT_OK(ParseScript(var_table, script));
  auto var = var_table->Lookup("join");
  ASSERT_EQ(var->type_descriptor().type(), QLObjectType::kDataframe);
  auto join_df = static_cast<Dataframe*>(var.get());

  // Check to make sure that the operator is a join.
  OperatorIR* op = join_df->op();
  ASSERT_MATCH(op, Join());

  JoinIR* join = static_cast<JoinIR*>(op);
  // Verify that the operator does what we expect it to.
  EXPECT_THAT(join->parents(), ElementsAre(src, src2));
  EXPECT_EQ(join->join_type(), JoinIR::JoinType::kInner);

  EXPECT_EQ(1, join->left_on_columns().size());
  EXPECT_EQ(1, join->right_on_columns().size());
  EXPECT_MATCH(join->left_on_columns()[0], ColumnNode("a", 0));
  EXPECT_MATCH(join->right_on_columns()[0], ColumnNode("b", 1));

  EXPECT_THAT(join->suffix_strs(), ElementsAre("_x", "_y"));
}

TEST_F(DataframeTest, Drop_WithList) {
  ASSERT_OK(ParseScript(var_table, "drop = df.drop(['foo', 'bar'])"));
  auto var = var_table->Lookup("drop");
  ASSERT_EQ(var->type_descriptor().type(), QLObjectType::kDataframe);
  auto df_obj = std::static_pointer_cast<Dataframe>(var);

  ASSERT_MATCH(df_obj->op(), Drop());
  DropIR* drop = static_cast<DropIR*>(df_obj->op());
  EXPECT_EQ(drop->col_names(), std::vector<std::string>({"foo", "bar"}));
}

TEST_F(DataframeTest, Drop_WithoutList) {
  ASSERT_OK(ParseScript(var_table, "drop = df.drop('foo')"));
  auto var = var_table->Lookup("drop");
  ASSERT_EQ(var->type_descriptor().type(), QLObjectType::kDataframe);
  auto df_obj = std::static_pointer_cast<Dataframe>(var);

  ASSERT_MATCH(df_obj->op(), Drop());
  DropIR* drop = static_cast<DropIR*>(df_obj->op());
  EXPECT_EQ(drop->col_names(), std::vector<std::string>({"foo"}));
}

TEST_F(DataframeTest, Drop_ErrorIfNotString) {
  EXPECT_THAT(ParseScript(var_table, "df.drop(1)"),
              HasCompilerError("Expected arg.*as type 'String', received 'Int"));
}

TEST_F(DataframeTest, Agg) {
  std::string script = R"pxl(agg = df.agg(
  outcol1=('col1', mean),
  outcol2=('col2', mean),
))pxl";
  ASSERT_OK(ParseScript(var_table, script));
  auto var = var_table->Lookup("agg");
  ASSERT_EQ(var->type_descriptor().type(), QLObjectType::kDataframe);
  OperatorIR* op = static_cast<Dataframe*>(var.get())->op();
  ASSERT_MATCH(op, BlockingAgg());
  BlockingAggIR* agg = static_cast<BlockingAggIR*>(op);
  EXPECT_THAT(agg->parents(), ElementsAre(src));
  ASSERT_EQ(agg->aggregate_expressions().size(), 2);
  std::vector<std::string> outcol_names{agg->aggregate_expressions()[0].name,
                                        agg->aggregate_expressions()[1].name};
  ASSERT_THAT(outcol_names, UnorderedElementsAre("outcol1", "outcol2"));

  std::vector<std::string> col_names;
  for (const auto& [index, expr] : Enumerate(agg->aggregate_expressions())) {
    ASSERT_TRUE(Match(expr.node, Func())) << index;
    FuncIR* fn = static_cast<FuncIR*>(expr.node);
    EXPECT_EQ(fn->func_name(), "mean") << index;
    ASSERT_EQ(fn->all_args().size(), 1) << index;
    ASSERT_TRUE(Match(fn->all_args()[0], ColumnNode())) << index;
    ColumnIR* col = static_cast<ColumnIR*>(fn->all_args()[0]);
    col_names.push_back(col->col_name());
  }
  ASSERT_THAT(col_names, UnorderedElementsAre("col1", "col2"));
}

TEST_F(DataframeTest, Agg_FailsWithPositionalArgs) {
  EXPECT_THAT(ParseScript(var_table, "df.agg(mean)"),
              HasCompilerError("agg.* takes 0 arguments but 1 .* given"));
}

TEST_F(DataframeTest, Agg_OnlyAllowTupleArgs) {
  EXPECT_THAT(ParseScript(var_table, "df.agg(outcol1=1)"),
              HasCompilerError("Expected tuple for outcol1 but received.*"));
}

TEST_F(DataframeTest, Agg_BadFirstTupleArg) {
  EXPECT_THAT(ParseScript(var_table, "df.agg(outcol1=(1, mean))"),
              HasCompilerError("All elements of the agg tuple must be column names, except the "
                               "last which should be a function"));
}

TEST_F(DataframeTest, Agg_BadSecondTupleArg) {
  EXPECT_THAT(ParseScript(var_table, "df.agg(outcol1=('ll', 'dd'))"),
              HasCompilerError("Expected second tuple argument to be type Func, received String"));
}

TEST_F(DataframeTest, CreateLimit) {
  ASSERT_OK(ParseScript(var_table, "limit = df.head(1234)"));
  auto var = var_table->Lookup("limit");
  ASSERT_EQ(var->type_descriptor().type(), QLObjectType::kDataframe);
  auto limit_obj = std::static_pointer_cast<Dataframe>(var);

  ASSERT_MATCH(limit_obj->op(), Limit());
  LimitIR* limit = static_cast<LimitIR*>(limit_obj->op());
  EXPECT_EQ(limit->limit_value(), 1234);
}

TEST_F(DataframeTest, LimitNonIntArgument) {
  EXPECT_THAT(ParseScript(var_table, "df.head('foo')"),
              HasCompilerError("Expected arg 'n' as type 'Int', received 'String'"));
}

TEST_F(DataframeTest, SubscriptFilterRows) {
  ASSERT_OK(ParseScript(var_table, "filter = df[df.service == 'blah']"));
  auto var = var_table->Lookup("filter");
  ASSERT_EQ(var->type_descriptor().type(), QLObjectType::kDataframe);
  auto filter_obj = std::static_pointer_cast<Dataframe>(var);
  FilterIR* filt_ir = static_cast<FilterIR*>(filter_obj->op());
  EXPECT_MATCH(filt_ir->filter_expr(), Equals(ColumnNode(), String()));
}

TEST_F(DataframeTest, SubscriptKeepColumns) {
  ASSERT_OK(ParseScript(var_table, "keep = df[['foo', 'baz']]"));
  auto var = var_table->Lookup("keep");
  ASSERT_EQ(var->type_descriptor().type(), QLObjectType::kDataframe);
  auto map_obj = std::static_pointer_cast<Dataframe>(var);
  MapIR* map_ir = static_cast<MapIR*>(map_obj->op());
  EXPECT_EQ(map_ir->col_exprs().size(), 2);
  EXPECT_EQ(map_ir->col_exprs()[0].name, "foo");
  EXPECT_EQ(map_ir->col_exprs()[1].name, "baz");
}

TEST_F(DataframeTest, SubscriptKeepMultipleOccurrences) {
  EXPECT_THAT(ParseScript(var_table, "df[['foo', 'foo']]"),
              HasCompilerError("cannot specify the same column name more than once when filtering "
                               "by cols. 'foo' specified more than once"));
}

TEST_F(DataframeTest, SubscriptCanHandleErrorInput) {
  // Call with an operator argument which is not allowed.
  EXPECT_THAT(
      ParseScript(var_table, "df[df]"),
      HasCompilerError(
          "subscript argument must have a list of strings or expression. '.*' not allowed"));
}

TEST_F(DataframeTest, SubscriptCreateColumn) {
  ASSERT_OK(ParseScript(var_table, "col = df['col1']"));
  auto var = var_table->Lookup("col");
  ASSERT_TRUE(ExprObject::IsExprObject(var));
  auto maybe_col_node = static_cast<ExprObject*>(var.get())->expr();
  ASSERT_MATCH(maybe_col_node, ColumnNode());
  EXPECT_EQ(static_cast<ColumnIR*>(maybe_col_node)->col_name(), "col1");
}

TEST_F(DataframeTest, GroupByList) {
  ASSERT_OK(ParseScript(var_table, "group = df.groupby(['col1', 'col2'])"));
  auto var = var_table->Lookup("group");
  ASSERT_EQ(var->type_descriptor().type(), QLObjectType::kDataframe);
  auto groupby_df = std::static_pointer_cast<Dataframe>(var);
  GroupByIR* group_by = static_cast<GroupByIR*>(groupby_df->op());
  EXPECT_EQ(group_by->groups().size(), 2);
  EXPECT_MATCH(group_by->groups()[0], ColumnNode("col1", 0));
  EXPECT_MATCH(group_by->groups()[1], ColumnNode("col2", 0));
}

TEST_F(DataframeTest, GroupByString) {
  ASSERT_OK(ParseScript(var_table, "group = df.groupby('col1')"));
  auto var = var_table->Lookup("group");
  ASSERT_EQ(var->type_descriptor().type(), QLObjectType::kDataframe);
  auto groupby_df = std::static_pointer_cast<Dataframe>(var);
  GroupByIR* group_by = static_cast<GroupByIR*>(groupby_df->op());
  EXPECT_EQ(group_by->groups().size(), 1);
  EXPECT_MATCH(group_by->groups()[0], ColumnNode("col1", 0));
}

TEST_F(DataframeTest, GroupByMixedListElementTypesCausesError) {
  EXPECT_THAT(ParseScript(var_table, "svc = df.groupby(['col1', 2])"),
              HasCompilerError("Expected arg 'by \\(index 1\\)' as type 'String', received 'Int'"));
}

TEST_F(DataframeTest, AttributeMetadataSubscriptTest) {
  ASSERT_OK(ParseScript(var_table, "svc = df.ctx['service']"));
  auto var = var_table->Lookup("svc");
  ASSERT_EQ(var->type_descriptor().type(), QLObjectType::kExpr);
  auto metadata_expr = static_cast<ExprObject*>(var.get());
  ASSERT_MATCH(metadata_expr->expr(), Metadata());
  auto metadata_node = static_cast<MetadataIR*>(metadata_expr->expr());
  EXPECT_EQ(metadata_node->name(), "service");
}

TEST_F(DataframeTest, UnionTest_array) {
  MemorySourceIR* src2 = MakeMemSource();
  MemorySourceIR* src3 = MakeMemSource();
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> df2,
                       Dataframe::Create(compiler_state.get(), src2, ast_visitor.get()));
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> df3,
                       Dataframe::Create(compiler_state.get(), src3, ast_visitor.get()));
  var_table->Add("df2", df2);
  var_table->Add("df3", df3);
  ASSERT_OK(ParseScript(var_table, "union = df.append([df2, df3])"));
  auto var = var_table->Lookup("union");

  ASSERT_EQ(var->type_descriptor().type(), QLObjectType::kDataframe);
  auto union_df = static_cast<Dataframe*>(var.get());

  // Check to make sure that the output is a Union operator.
  OperatorIR* op = union_df->op();
  ASSERT_MATCH(op, Union());
  EXPECT_THAT(op->parents(), ElementsAre(src, src2, src3));
}

TEST_F(DataframeTest, UnionTest_single) {
  MemorySourceIR* src2 = MakeMemSource();
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> df2,
                       Dataframe::Create(compiler_state.get(), src2, ast_visitor.get()));
  var_table->Add("df2", df2);
  ASSERT_OK(ParseScript(var_table, "union = df.append(df2)"));
  auto var = var_table->Lookup("union");

  ASSERT_EQ(var->type_descriptor().type(), QLObjectType::kDataframe);
  auto union_df = static_cast<Dataframe*>(var.get());

  // Check to make sure that the output is a Union operator.
  OperatorIR* op = union_df->op();
  ASSERT_MATCH(op, Union());
  EXPECT_THAT(op->parents(), ElementsAre(src, src2));
}

TEST_F(DataframeTest, ConstructorTest) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<Dataframe> df,
                       Dataframe::Create(compiler_state.get(), graph.get(), ast_visitor.get()));
  var_table->Add("DataFrame", df);
  ASSERT_OK(ParseScript(var_table, "http = DataFrame('http_events')"));
  auto var = var_table->Lookup("http");

  ASSERT_EQ(var->type_descriptor().type(), QLObjectType::kDataframe);
  auto df_obj = static_cast<Dataframe*>(var.get());

  // Check to make sure that the operator is a MemorySource.
  ASSERT_MATCH(df_obj->op(), MemorySource());
  MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(df_obj->op());
  EXPECT_EQ(mem_src->table_name(), "http_events");
}

TEST_F(DataframeTest, StreamTest) {
  ASSERT_OK(ParseScript(var_table, "s = df.stream()"));
  auto var = var_table->Lookup("s");
  ASSERT_EQ(var->type_descriptor().type(), QLObjectType::kDataframe);
  auto streamdf = static_cast<Dataframe*>(var.get());
  ASSERT_MATCH(streamdf->op(), Stream());
  EXPECT_THAT(streamdf->op()->parents(), ElementsAre(src));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
