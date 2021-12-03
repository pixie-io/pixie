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
#include "src/carnot/planner/objects/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

class DataframeTest : public QLObjectTest {
 protected:
  void SetUp() override {
    QLObjectTest::SetUp();
    src = MakeMemSource();
    ASSERT_OK_AND_ASSIGN(df, Dataframe::Create(src, ast_visitor.get()));
  }

  std::shared_ptr<Dataframe> df;
  MemorySourceIR* src;
};

TEST_F(DataframeTest, Merge_ListKeys) {
  MemorySourceIR* src2 = MakeMemSource();
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> df2, Dataframe::Create(src2, ast_visitor.get()));

  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj, df->GetMethod(Dataframe::kMergeOpID));
  ArgMap args{{},
              {df2, ToQLObject(MakeString("inner")), MakeListObj(MakeString("a"), MakeString("b")),
               MakeListObj(MakeString("b"), MakeString("c")),
               MakeListObj(MakeString("_x"), MakeString("_y"))}};
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> obj, func_obj->Call(args, ast));
  ASSERT_EQ(obj->type_descriptor().type(), QLObjectType::kDataframe);
  auto join_df = static_cast<Dataframe*>(obj.get());

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
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> df2, Dataframe::Create(src2, ast_visitor.get()));

  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj, df->GetMethod(Dataframe::kMergeOpID));
  ArgMap args{{},
              {df2, ToQLObject(MakeString("inner")), ToQLObject(MakeString("a")),
               ToQLObject(MakeString("b")), MakeListObj(MakeString("_x"), MakeString("_y"))}};
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> obj, func_obj->Call(args, ast));
  ASSERT_EQ(obj->type_descriptor().type(), QLObjectType::kDataframe);
  auto join_df = static_cast<Dataframe*>(obj.get());

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
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj, df->GetMethod(Dataframe::kDropOpID));
  ArgMap args{{}, {MakeListObj(MakeString("foo"), MakeString("bar"))}};
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> obj, func_obj->Call(args, ast));
  ASSERT_EQ(obj->type_descriptor().type(), QLObjectType::kDataframe);
  auto df_obj = std::static_pointer_cast<Dataframe>(obj);

  ASSERT_MATCH(df_obj->op(), Drop());
  DropIR* drop = static_cast<DropIR*>(df_obj->op());
  EXPECT_EQ(drop->col_names(), std::vector<std::string>({"foo", "bar"}));
}

TEST_F(DataframeTest, Drop_WithoutList) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj, df->GetMethod(Dataframe::kDropOpID));
  ArgMap args{{}, {ToQLObject(MakeString("foo"))}};
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> obj, func_obj->Call(args, ast));
  ASSERT_EQ(obj->type_descriptor().type(), QLObjectType::kDataframe);
  auto df_obj = std::static_pointer_cast<Dataframe>(obj);

  ASSERT_MATCH(df_obj->op(), Drop());
  DropIR* drop = static_cast<DropIR*>(df_obj->op());
  EXPECT_EQ(drop->col_names(), std::vector<std::string>({"foo"}));
}

TEST_F(DataframeTest, Drop_ErrorIfNotString) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj, df->GetMethod(Dataframe::kDropOpID));
  ArgMap args{{}, {MakeListObj(MakeInt(1))}};
  EXPECT_THAT(
      func_obj->Call(args, ast).status(),
      HasCompilerError("Expected arg 'columns \\(index 0\\)' as type 'String', received 'Int"));
}

StatusOr<QLObjectPtr> UDFHandler(IR* graph, const std::string& name, const pypa::AstPtr& ast,
                                 const ParsedArgs&, ASTVisitor* visitor) {
  PL_ASSIGN_OR_RETURN(FuncIR * node,
                      graph->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", name},
                                                std::vector<ExpressionIR*>{}));
  return ExprObject::Create(node, visitor);
}

std::shared_ptr<FuncObject> MakeUDFFunc(ASTVisitor* visitor, IR* graph,
                                        std::string_view func_name) {
  return FuncObject::Create(
             func_name, {}, {},
             /* has_variable_len_args */ true,
             /* has_variable_len_kwargs */ false,
             std::bind(&UDFHandler, graph, std::string(func_name), std::placeholders::_1,
                       std::placeholders::_2, std::placeholders::_3),
             visitor)
      .ConsumeValueOrDie();
}

TEST_F(DataframeTest, Agg) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj,
                       df->GetMethod(Dataframe::kBlockingAggOpID));
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> tuple1,
                       TupleObject::Create({ToQLObject(MakeString("col1")),
                                            MakeUDFFunc(ast_visitor.get(), src->graph(), "mean")},
                                           ast_visitor.get()));
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> tuple2,
                       TupleObject::Create({ToQLObject(MakeString("col2")),
                                            MakeUDFFunc(ast_visitor.get(), src->graph(), "mean")},
                                           ast_visitor.get()));
  ArgMap args{{{"outcol1", tuple1}, {"outcol2", tuple2}}, {}};
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> obj, func_obj->Call(args, ast));
  ASSERT_EQ(obj->type_descriptor().type(), QLObjectType::kDataframe);
  OperatorIR* op = static_cast<Dataframe*>(obj.get())->op();
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
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj,
                       df->GetMethod(Dataframe::kBlockingAggOpID));
  ArgMap args{{}, {ToQLObject(MakeMeanFunc())}};
  EXPECT_THAT(func_obj->Call(args, ast).status(),
              HasCompilerError("agg.* takes 0 arguments but 1 .* given"));
}

TEST_F(DataframeTest, Agg_OnlyAllowTupleArgs) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj,
                       df->GetMethod(Dataframe::kBlockingAggOpID));
  ArgMap args{{{"outcol1", ToQLObject(MakeMeanFunc())}}, {}};
  EXPECT_THAT(func_obj->Call(args, ast).status(),
              HasCompilerError("Expected tuple for value at kwarg outcol1 but received .*"));
}

TEST_F(DataframeTest, Agg_BadFirstTupleArg) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj,
                       df->GetMethod(Dataframe::kBlockingAggOpID));
  ArgMap args{{{"outcol1", MakeTupleObj(MakeInt(1), MakeMeanFunc())}}, {}};
  EXPECT_THAT(func_obj->Call(args, ast).status(),
              HasCompilerError("All elements of the agg tuple must be column names, except the "
                               "last which should be a function"));
}

TEST_F(DataframeTest, Agg_BadSecondTupleArg) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj,
                       df->GetMethod(Dataframe::kBlockingAggOpID));
  ArgMap args{{{"outcol1", MakeTupleObj(MakeString("ll"), MakeString("dd"))}}, {}};
  EXPECT_THAT(func_obj->Call(args, ast).status(),
              HasCompilerError("Expected second tuple argument to be type Func, received String"));
}

TEST_F(DataframeTest, CreateLimit) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj, df->GetMethod(Dataframe::kLimitOpID));
  ArgMap args{{}, {ToQLObject(MakeInt(1234))}};
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> obj, func_obj->Call(args, ast));
  ASSERT_EQ(obj->type_descriptor().type(), QLObjectType::kDataframe);
  auto limit_obj = std::static_pointer_cast<Dataframe>(obj);

  ASSERT_MATCH(limit_obj->op(), Limit());
  LimitIR* limit = static_cast<LimitIR*>(limit_obj->op());
  EXPECT_EQ(limit->limit_value(), 1234);
}

TEST_F(DataframeTest, LimitNonIntArgument) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj, df->GetMethod(Dataframe::kLimitOpID));
  ArgMap args{{}, {ToQLObject(MakeString("1234"))}};
  EXPECT_THAT(func_obj->Call(args, ast).status(),
              HasCompilerError("Expected arg 'n' as type 'Int', received 'String'"));
}

TEST_F(DataframeTest, SubscriptFilterRows) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj, df->GetSubscriptMethod());
  auto eq_func = MakeEqualsFunc(MakeColumn("service", 0), MakeString("blah"));
  ArgMap args{{}, {ToQLObject(eq_func)}};
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> obj, func_obj->Call(args, ast));
  ASSERT_EQ(obj->type_descriptor().type(), QLObjectType::kDataframe);
  auto filter_obj = std::static_pointer_cast<Dataframe>(obj);
  FilterIR* filt_ir = static_cast<FilterIR*>(filter_obj->op());
  EXPECT_EQ(filt_ir->filter_expr(), eq_func);
}

TEST_F(DataframeTest, SubscriptKeepColumns) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj, df->GetSubscriptMethod());
  ArgMap args{{}, {MakeListObj(MakeString("foo"), MakeString("bar"))}};
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> obj, func_obj->Call(args, ast));
  ASSERT_EQ(obj->type_descriptor().type(), QLObjectType::kDataframe);
  auto map_obj = std::static_pointer_cast<Dataframe>(obj);
  MapIR* map_ir = static_cast<MapIR*>(map_obj->op());
  EXPECT_EQ(map_ir->col_exprs().size(), 2);
  EXPECT_EQ(map_ir->col_exprs()[0].name, "foo");
  EXPECT_EQ(map_ir->col_exprs()[1].name, "bar");
}

TEST_F(DataframeTest, SbuscriptKeepMultipleOccurrences) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj, df->GetSubscriptMethod());
  ArgMap args{{}, {MakeListObj(MakeString("foo"), MakeString("foo"))}};
  EXPECT_THAT(func_obj->Call(args, ast).status(),
              HasCompilerError("cannot specify the same column name more than once when filtering "
                               "by cols. 'foo' specified more than once"));
}

TEST_F(DataframeTest, SubscriptCanHandleErrorInput) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj, df->GetSubscriptMethod());
  // Call with an operator argument which is not allowed.
  EXPECT_THAT(
      func_obj->Call({{}, {ToQLObject(MakeMemSource())}}, ast).status(),
      HasCompilerError(
          "subscript argument must have a list of strings or expression. '.*' not allowed"));
}

TEST_F(DataframeTest, SubscriptCreateColumn) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj, df->GetSubscriptMethod());
  ArgMap args{{}, {ToQLObject(MakeString("col1"))}};
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> obj, func_obj->Call(args, ast));
  ASSERT_EQ(obj->type_descriptor().type(), QLObjectType::kExpr);
  auto maybe_col_node = obj->node();
  ASSERT_MATCH(maybe_col_node, ColumnNode());
  EXPECT_EQ(static_cast<ColumnIR*>(maybe_col_node)->col_name(), "col1");
}

TEST_F(DataframeTest, GroupByList) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj,
                       df->GetMethod(Dataframe::kGroupByOpID));
  ASSERT_OK_AND_ASSIGN(
      std::shared_ptr<QLObject> list,
      ListObject::Create({ToQLObject(MakeString("col1")), ToQLObject(MakeString("col2"))},
                         ast_visitor.get()));
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> obj, func_obj->Call({{}, {list}}, ast));
  ASSERT_EQ(obj->type_descriptor().type(), QLObjectType::kDataframe);
  auto groupby_df = std::static_pointer_cast<Dataframe>(obj);
  GroupByIR* group_by = static_cast<GroupByIR*>(groupby_df->op());
  EXPECT_EQ(group_by->groups().size(), 2);
  EXPECT_MATCH(group_by->groups()[0], ColumnNode("col1", 0));
  EXPECT_MATCH(group_by->groups()[1], ColumnNode("col2", 0));
}

TEST_F(DataframeTest, GroupByString) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj,
                       df->GetMethod(Dataframe::kGroupByOpID));
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> obj,
                       func_obj->Call(MakeArgMap({}, {MakeString("col1")}), ast));
  ASSERT_EQ(obj->type_descriptor().type(), QLObjectType::kDataframe);
  auto groupby_df = std::static_pointer_cast<Dataframe>(obj);
  GroupByIR* group_by = static_cast<GroupByIR*>(groupby_df->op());
  EXPECT_EQ(group_by->groups().size(), 1);
  EXPECT_MATCH(group_by->groups()[0], ColumnNode("col1", 0));
}

TEST_F(DataframeTest, GroupByMixedListElementTypesCausesError) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj,
                       df->GetMethod(Dataframe::kGroupByOpID));
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> list,
                       ListObject::Create({ToQLObject(MakeString("col1")), ToQLObject(MakeInt(2))},
                                          ast_visitor.get()));
  EXPECT_THAT(func_obj->Call({{}, {list}}, ast).status(),
              HasCompilerError("Expected arg 'by \\(index 1\\)' as type 'String', received 'Int'"));
}

TEST_F(DataframeTest, AttributeMetadataSubscriptTest) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> md_obj,
                       df->GetAttribute(ast, Dataframe::kMetadataAttrName));
  ASSERT_EQ(md_obj->type_descriptor().type(), QLObjectType::kMetadata);

  auto metadata = static_cast<MetadataObject*>(md_obj.get());
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> subscript_fn, metadata->GetSubscriptMethod());
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> subscript_obj,
                       subscript_fn->Call(MakeArgMap({}, {MakeString("service")}), ast));
  ASSERT_EQ(subscript_obj->type_descriptor().type(), QLObjectType::kExpr);
  auto metadata_expr = static_cast<ExprObject*>(subscript_obj.get());
  ASSERT_TRUE(metadata_expr->HasNode());
  ASSERT_MATCH(metadata_expr->node(), Metadata());
  auto metadata_node = static_cast<MetadataIR*>(metadata_expr->node());
  EXPECT_EQ(metadata_node->name(), "service");
}

TEST_F(DataframeTest, UnionTest_array) {
  MemorySourceIR* src2 = MakeMemSource();
  MemorySourceIR* src3 = MakeMemSource();
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> df2, Dataframe::Create(src2, ast_visitor.get()));
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> df3, Dataframe::Create(src3, ast_visitor.get()));

  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> union_fn, df->GetMethod(Dataframe::kUnionOpID));
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> df_list,
                       ListObject::Create({df2, df3}, ast_visitor.get()));
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> obj, union_fn->Call({{}, {df_list}}, ast));
  ASSERT_EQ(obj->type_descriptor().type(), QLObjectType::kDataframe);
  auto union_df = static_cast<Dataframe*>(obj.get());

  // Check to make sure that the operator is a Union.
  OperatorIR* op = union_df->op();
  ASSERT_MATCH(op, Union());
  EXPECT_THAT(op->parents(), ElementsAre(src, src2, src3));
}

TEST_F(DataframeTest, UnionTest_single) {
  MemorySourceIR* src1 = MakeMemSource();
  MemorySourceIR* src2 = MakeMemSource();
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> df1, Dataframe::Create(src1, ast_visitor.get()));
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> df2, Dataframe::Create(src2, ast_visitor.get()));

  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> union_fn, df1->GetMethod(Dataframe::kUnionOpID));
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> obj, union_fn->Call({{}, {df2}}, ast));
  ASSERT_EQ(obj->type_descriptor().type(), QLObjectType::kDataframe);
  auto union_df = static_cast<Dataframe*>(obj.get());

  // Check to make sure that the output is a Union operator.
  OperatorIR* op = union_df->op();
  ASSERT_MATCH(op, Union());
  EXPECT_THAT(op->parents(), ElementsAre(src1, src2));
}

TEST_F(DataframeTest, ConstructorTest) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<Dataframe> df,
                       Dataframe::Create(graph.get(), ast_visitor.get()));

  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> func_obj, df->GetCallMethod());

  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> obj,
                       func_obj->Call(MakeArgMap({}, {MakeString("http_events")}), ast));
  // Add compartor for type() and Dataframe.
  ASSERT_EQ(obj->type_descriptor().type(), QLObjectType::kDataframe);
  auto df_obj = static_cast<Dataframe*>(obj.get());

  // Check to make sure that the operator is a MemorySource.
  ASSERT_MATCH(df_obj->op(), MemorySource());
  MemorySourceIR* mem_src = static_cast<MemorySourceIR*>(df_obj->op());
  EXPECT_EQ(mem_src->table_name(), "http_events");
}

TEST_F(DataframeTest, StreamTest) {
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<FuncObject> streamfn, df->GetMethod(Dataframe::kStreamOpId));
  ASSERT_OK_AND_ASSIGN(std::shared_ptr<QLObject> obj, streamfn->Call({{}, {}}, ast));

  ASSERT_EQ(obj->type_descriptor().type(), QLObjectType::kDataframe);
  auto streamdf = static_cast<Dataframe*>(obj.get());

  // Check to make sure that the operator is a Stream.
  ASSERT_MATCH(streamdf->op(), Stream());
  EXPECT_THAT(streamdf->op()->parents(), ElementsAre(src));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
