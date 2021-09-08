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

#include <vector>

#include <gtest/gtest.h>

#include "src/carnot/planner/compiler/analyzer/propagate_expression_annotations_rule.h"
#include "src/carnot/planner/compiler/analyzer/resolve_types_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using PropagateExpressionAnnotationsRuleTest = RulesTest;

TEST_F(PropagateExpressionAnnotationsRuleTest, noop) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource("source", relation);
  compiler_state_->relation_map()->emplace("source", relation);
  MapIR* map1 = MakeMap(src, {{"def", MakeColumn("abc", 0)}}, false);
  MapIR* map2 = MakeMap(map1, {{"xyz", MakeInt(3)}, {"def", MakeColumn("def", 0)}}, false);
  auto eq_func = MakeEqualsFunc(MakeColumn("def", 0), MakeInt(2));
  FilterIR* filter = MakeFilter(map2, eq_func);
  MakeMemSink(filter, "foo", {});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  PropagateExpressionAnnotationsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
}

TEST_F(PropagateExpressionAnnotationsRuleTest, rename) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource("source", relation);
  compiler_state_->relation_map()->emplace("source", relation);
  auto map1_col = MakeColumn("abc", 0);
  auto annotations = ExpressionIR::Annotations(MetadataType::POD_NAME);
  map1_col->set_annotations(annotations);

  MapIR* map1 = MakeMap(src, {{"def", map1_col}}, false);
  auto map2_col1 = MakeColumn("def", 0);
  auto map2_col2 = MakeColumn("def", 0);
  MapIR* map2 = MakeMap(map1, {{"xyz", map2_col1}, {"def", map2_col2}, {"ghi", MakeInt(2)}}, false);
  auto filter_col1 = MakeColumn("xyz", 0);
  auto filter_col2 = MakeColumn("ghi", 0);
  auto eq_func = MakeEqualsFunc(filter_col1, filter_col2);
  FilterIR* filter = MakeFilter(map2, eq_func);
  MakeMemSink(filter, "foo", {});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto default_annotations = ExpressionIR::Annotations();
  EXPECT_EQ(default_annotations, map2_col1->annotations());
  EXPECT_EQ(default_annotations, map2_col2->annotations());
  ASSERT_MATCH(filter->filter_expr(), Func());
  auto filter_func = static_cast<FuncIR*>(filter->filter_expr());
  EXPECT_EQ(default_annotations, filter_func->args()[0]->annotations());

  PropagateExpressionAnnotationsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_EQ(annotations, map1_col->annotations());
  EXPECT_EQ(annotations, map2_col1->annotations());
  EXPECT_EQ(annotations, map2_col2->annotations());
  EXPECT_EQ(annotations, filter_func->args()[0]->annotations());
  EXPECT_EQ(default_annotations, filter_func->args()[1]->annotations());
}

TEST_F(PropagateExpressionAnnotationsRuleTest, join) {
  std::string join_key = "key";
  Relation rel1({types::FLOAT64, types::STRING}, {"latency", "data"});
  Relation rel2({types::STRING, types::FLOAT64}, {join_key, "cpu_usage"});

  auto mem_src1 = MakeMemSource("table1", rel1);
  compiler_state_->relation_map()->emplace("table1", rel1);
  auto literal_with_annotations = MakeString("my_pod_name");
  auto annotations = ExpressionIR::Annotations(MetadataType::POD_NAME);
  literal_with_annotations->set_annotations(annotations);

  auto map = MakeMap(mem_src1, {
                                   {join_key, literal_with_annotations},
                                   {"latency", MakeColumn("latency", 0)},
                                   {"data", MakeColumn("data", 0)},
                               });

  auto mem_src2 = MakeMemSource("table2", rel2);
  compiler_state_->relation_map()->emplace("table2", rel2);

  std::string left_suffix = "_x";
  std::string right_suffix = "_y";

  JoinIR* join = graph
                     ->CreateNode<JoinIR>(ast, std::vector<OperatorIR*>{map, mem_src2}, "inner",
                                          std::vector<ColumnIR*>{MakeColumn(join_key, 0)},
                                          std::vector<ColumnIR*>{MakeColumn(join_key, 1)},
                                          std::vector<std::string>{left_suffix, right_suffix})
                     .ConsumeValueOrDie();
  auto map_col1 = MakeColumn("key_x", 0);
  auto map_col2 = MakeColumn("latency", 0);
  auto last = MakeMap(join, {{"annotations_col", map_col1}, {"non_annotations_col", map_col2}});
  MakeMemSink(last, "foo", {});

  // run ResolveTypes rule to get types, since this rule will run before
  // PropagateExpressionAnnotationsRule.
  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto default_annotations = ExpressionIR::Annotations();

  EXPECT_THAT(*join->resolved_table_type(),
              IsTableType(Relation(
                  {types::STRING, types::FLOAT64, types::STRING, types::STRING, types::FLOAT64},
                  {"key_x", "latency", "data", "key_y", "cpu_usage"})));

  PropagateExpressionAnnotationsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  ASSERT_TRUE(result.ValueOrDie());

  EXPECT_EQ(annotations, join->output_columns()[0]->annotations());
  for (size_t i = 1; i < join->output_columns().size(); ++i) {
    EXPECT_EQ(default_annotations, join->output_columns()[i]->annotations());
  }
  EXPECT_EQ(annotations, map_col1->annotations());
  EXPECT_EQ(default_annotations, map_col2->annotations());
}

TEST_F(PropagateExpressionAnnotationsRuleTest, agg) {
  Relation relation({types::DataType::INT64, types::DataType::INT64}, {"abc", "xyz"});
  MemorySourceIR* src = MakeMemSource("source", relation);
  compiler_state_->relation_map()->emplace("source", relation);
  // Set up the columns and their annotations.
  auto group_col = MakeColumn("abc", 0);
  auto agg_col = MakeColumn("xyz", 0);
  auto agg_func = MakeMeanFunc(agg_col);
  auto group_col_annotation = ExpressionIR::Annotations(MetadataType::POD_NAME);
  auto agg_col_annotation = ExpressionIR::Annotations(MetadataType::SERVICE_ID);
  auto agg_func_annotation = ExpressionIR::Annotations(MetadataType::POD_ID);
  group_col->set_annotations(group_col_annotation);
  agg_col->set_annotations(agg_col_annotation);
  agg_func->set_annotations(agg_func_annotation);

  BlockingAggIR* agg = MakeBlockingAgg(src, {group_col}, {{"out", agg_func}});
  auto filter_col = MakeColumn("out", 0);
  auto eq_func = MakeEqualsFunc(filter_col, MakeInt(2));
  FilterIR* filter = MakeFilter(agg, eq_func);
  auto map_expr_col = MakeColumn("out", 0);
  auto map_group_col = MakeColumn("abc", 0);
  MapIR* map = MakeMap(filter, {{"agg_expr", map_expr_col}, {"agg_group", map_group_col}});
  MakeMemSink(map, "");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  PropagateExpressionAnnotationsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_EQ(agg_func_annotation, filter_col->annotations());
  EXPECT_EQ(agg_func_annotation, map_expr_col->annotations());
  EXPECT_EQ(group_col_annotation, map_group_col->annotations());
}

TEST_F(PropagateExpressionAnnotationsRuleTest, union) {
  // Test to make sure that union columns that share annotations produce those annotations in the
  // union output, whereas annotations that are not shared are not produced in the output.
  Relation relation1({types::DataType::STRING, types::DataType::STRING}, {"pod_id", "pod_name"});
  Relation relation2({types::DataType::STRING, types::DataType::STRING},
                     {"pod_id", "random_string"});
  auto mem_src1 = MakeMemSource("table1", relation1);
  compiler_state_->relation_map()->emplace("table1", relation1);
  auto mem_src2 = MakeMemSource("table2", relation2);
  compiler_state_->relation_map()->emplace("table2", relation2);

  auto map1_col1 = MakeColumn("pod_id", 0);
  auto map1_col2 = MakeColumn("pod_name", 0);
  auto map2_col1 = MakeColumn("pod_id", 0);
  auto map2_col2 = MakeColumn("random_string", 0);

  auto map1 = MakeMap(mem_src1, {{"pod_id", map1_col1}, {"maybe_pod_name", map1_col2}});
  auto map2 = MakeMap(mem_src2, {{"pod_id", map2_col1}, {"maybe_pod_name", map2_col2}});

  auto union_op = MakeUnion({map1, map2});

  auto map3_col1 = MakeColumn("pod_id", 0);
  auto map3_col2 = MakeColumn("maybe_pod_name", 0);
  MakeMap(union_op, {{"pod_id", map3_col1}, {"maybe_pod_name", map3_col2}});

  // add metadata:
  auto pod_id_annotation = ExpressionIR::Annotations(MetadataType::POD_ID);
  auto pod_name_annotation = ExpressionIR::Annotations(MetadataType::POD_NAME);
  auto default_annotation = ExpressionIR::Annotations();
  map1_col1->set_annotations(pod_id_annotation);
  map2_col1->set_annotations(pod_id_annotation);
  map1_col2->set_annotations(pod_name_annotation);

  EXPECT_EQ(default_annotation, map3_col1->annotations());
  EXPECT_EQ(default_annotation, map3_col2->annotations());

  // run ResolveTypes rule to get types, since this rule will run before
  // PropagateExpressionAnnotationsRule.
  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  PropagateExpressionAnnotationsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_EQ(pod_id_annotation, map3_col1->annotations());
  EXPECT_EQ(default_annotation, map3_col2->annotations());
}

TEST_F(PropagateExpressionAnnotationsRuleTest, filter_limit) {
  Relation relation({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64},
                    {"abc", "xyz", "column"});
  MemorySourceIR* src = MakeMemSource("source", relation);
  compiler_state_->relation_map()->emplace("source", relation);

  auto map1_col = MakeColumn("abc", 0);
  auto annotations = ExpressionIR::Annotations(MetadataType::POD_NAME);
  auto default_annotation = ExpressionIR::Annotations();
  map1_col->set_annotations(annotations);

  auto map1 = MakeMap(
      src,
      {{"abc_1", map1_col}, {"xyz_1", MakeColumn("xyz", 0)}, {"column", MakeColumn("column", 0)}});
  auto limit1 = MakeLimit(map1, 100);
  auto filter1 = MakeFilter(limit1);
  auto limit2 = MakeLimit(filter1, 10);
  auto filter2 = MakeFilter(limit2);

  auto map1_col1 = MakeColumn("abc_1", 0);
  auto map1_col2 = MakeColumn("xyz_1", 0);
  MakeMap(filter2, {{"foo", map1_col2}, {"bar", map1_col1}});

  EXPECT_EQ(default_annotation, map1_col1->annotations());
  EXPECT_EQ(default_annotation, map1_col2->annotations());

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  PropagateExpressionAnnotationsRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_EQ(annotations, map1_col1->annotations());
  EXPECT_EQ(default_annotation, map1_col2->annotations());
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
