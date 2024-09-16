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

#include "src/carnot/planner/compiler/analyzer/convert_metadata_rule.h"
#include "src/carnot/planner/compiler/analyzer/resolve_types_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using table_store::schema::Relation;

using ConvertMetadataRuleTest = RulesTest;

TEST_F(ConvertMetadataRuleTest, multichild_without_fallback_func) {
  auto relation = Relation(cpu_relation);
  MetadataType conversion_column = MetadataType::UPID;
  std::string conversion_column_str = MetadataProperty::GetMetadataString(conversion_column);
  relation.AddColumn(types::DataType::UINT128, conversion_column_str);
  compiler_state_->relation_map()->emplace("table", relation);

  auto metadata_name = "pod_name";
  MetadataProperty* property = md_handler->GetProperty(metadata_name).ValueOrDie();
  MetadataIR* metadata_ir = MakeMetadataIR(metadata_name, /* parent_op_idx */ 0);
  metadata_ir->set_property(property);

  auto src = MakeMemSource(relation);
  auto map1 = MakeMap(src, {{"md", metadata_ir}});
  auto map2 = MakeMap(src, {{"other_col", MakeInt(2)}, {"md", metadata_ir}});
  auto filter = MakeFilter(src, MakeEqualsFunc(metadata_ir, MakeString("pl/foobar")));

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  ConvertMetadataRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_EQ(0, graph->FindNodesThatMatch(Metadata()).size());

  // Check the contents of the new func.
  EXPECT_MATCH(filter->filter_expr(), Equals(Func(), String()));
  auto converted_md = static_cast<FuncIR*>(filter->filter_expr())->all_args()[0];
  EXPECT_MATCH(converted_md, Func());
  auto converted_md_func = static_cast<FuncIR*>(converted_md);
  EXPECT_EQ(absl::Substitute("upid_to_$0", metadata_name), converted_md_func->func_name());
  EXPECT_EQ(1, converted_md_func->all_args().size());
  auto input_col = converted_md_func->all_args()[0];
  EXPECT_MATCH(input_col, ColumnNode("upid"));

  EXPECT_MATCH(converted_md, ResolvedExpression());
  EXPECT_MATCH(input_col, ResolvedExpression());
  EXPECT_EQ(types::DataType::STRING, converted_md->EvaluatedDataType());
  EXPECT_EQ(types::DataType::UINT128, input_col->EvaluatedDataType());
  EXPECT_EQ(ExpressionIR::Annotations(MetadataType::POD_NAME), converted_md->annotations());
  EXPECT_EQ(1, converted_md_func->func_id());

  // Check to make sure that all of the operators and expressions depending on the metadata
  // now have an updated reference to the func.
  EXPECT_EQ(converted_md, map1->col_exprs()[0].node);
  EXPECT_EQ(converted_md, map2->col_exprs()[1].node);

  // Check that the semantic type of the conversion func is propagated properly.
  auto type_or_s = map2->resolved_table_type()->GetColumnType("md");
  ASSERT_OK(type_or_s);
  auto type = std::static_pointer_cast<ValueType>(type_or_s.ConsumeValueOrDie());
  EXPECT_EQ(types::STRING, type->data_type());
  EXPECT_EQ(types::ST_POD_NAME, type->semantic_type());
}

TEST_F(ConvertMetadataRuleTest, missing_conversion_column) {
  auto relation = table_store::schema::Relation(cpu_relation);
  compiler_state_->relation_map()->emplace("table", relation);

  auto metadata_name = "pod_name";
  NameMetadataProperty property(MetadataType::POD_NAME, {MetadataType::UPID});
  MetadataIR* metadata_ir = MakeMetadataIR(metadata_name, /* parent_op_idx */ 0);
  metadata_ir->set_property(&property);
  MakeMap(MakeMemSource(relation), {{"md", metadata_ir}});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  ConvertMetadataRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  EXPECT_NOT_OK(result);
  VLOG(1) << result.ToString();
  EXPECT_THAT(result.status(),
              HasCompilerError(
                  "Can\'t resolve metadata because of lack of converting columns in the parent. "
                  "Need one of "
                  "\\[upid\\]. Parent type has columns \\[count,cpu0,cpu1,cpu2\\] available."));

  skip_check_stray_nodes_ = true;
}

TEST_F(ConvertMetadataRuleTest, multichild_with_fallback_func) {
  auto relation = Relation(http_events_relation);
  MetadataType conversion_column = MetadataType::UPID;
  std::string conversion_column_str = MetadataProperty::GetMetadataString(conversion_column);
  relation.AddColumn(types::DataType::UINT128, conversion_column_str);
  compiler_state_->relation_map()->emplace("table", relation);

  auto metadata_name = "pod_name";
  MetadataProperty* property = md_handler->GetProperty(metadata_name).ValueOrDie();
  MetadataIR* metadata_ir = MakeMetadataIR(metadata_name, /* parent_op_idx */ 0);
  metadata_ir->set_property(property);

  auto src = MakeMemSource(relation);
  auto map = MakeMap(src, {{"md", metadata_ir}});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  ConvertMetadataRule rule(compiler_state_.get());
  auto result = rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  EXPECT_EQ(0, graph->FindNodesThatMatch(Metadata()).size());

  EXPECT_EQ(1, src->Children().size());
  auto md_map = static_cast<MapIR*>(src->Children()[0]);
  EXPECT_NE(md_map, map);

  FuncIR* upid_to_pod_name = nullptr;
  for (auto col_expr : md_map->col_exprs()) {
    if (col_expr.name == "pod_name_0") {
      EXPECT_MATCH(col_expr.node, Func());
      upid_to_pod_name = static_cast<FuncIR*>(col_expr.node);
    }
  }
  EXPECT_NE(upid_to_pod_name, nullptr);
  EXPECT_EQ(absl::Substitute("upid_to_$0", metadata_name), upid_to_pod_name->func_name());
  EXPECT_EQ(1, upid_to_pod_name->all_args().size());
  auto input_col = upid_to_pod_name->all_args()[0];
  EXPECT_MATCH(input_col, ColumnNode("upid"));
  EXPECT_MATCH(upid_to_pod_name, ResolvedExpression());
  EXPECT_MATCH(input_col, ResolvedExpression());

  EXPECT_EQ(1, md_map->Children().size());
  auto fallback_map = static_cast<MapIR*>(md_map->Children()[0]);
  FuncIR* fallback_func_select = nullptr;
  for (auto col_expr : fallback_map->col_exprs()) {
    if (col_expr.name == "pod_name_1") {
      EXPECT_MATCH(col_expr.node, Func());
      fallback_func_select = static_cast<FuncIR*>(col_expr.node);
    }
  }

  EXPECT_NE(fallback_func_select, nullptr);
  EXPECT_EQ("select", fallback_func_select->func_name());
  EXPECT_EQ(3, fallback_func_select->all_args().size());

  auto orig_func_check = fallback_func_select->all_args()[0];
  EXPECT_MATCH(orig_func_check, Func());
  auto equals_func = static_cast<FuncIR*>(orig_func_check);
  EXPECT_EQ("equal", equals_func->func_name());
  EXPECT_EQ(2, equals_func->all_args().size());
  EXPECT_MATCH(equals_func->all_args()[0], ColumnNode("pod_name_0"));
  EXPECT_MATCH(equals_func->all_args()[1], String(""));
  EXPECT_MATCH(orig_func_check, ResolvedExpression());

  EXPECT_MATCH(fallback_func_select->all_args()[1], Func());
  auto fallback_func = static_cast<FuncIR*>(fallback_func_select->all_args()[1]);
  EXPECT_EQ("pod_id_to_pod_name", fallback_func->func_name());
  EXPECT_EQ(1, fallback_func->all_args().size());
  EXPECT_MATCH(fallback_func->all_args()[0], Func());
  EXPECT_MATCH(fallback_func, ResolvedExpression());

  auto ip_func = static_cast<FuncIR*>(fallback_func->all_args()[0]);
  EXPECT_EQ("_ip_to_pod_id_pem_exec", ip_func->func_name());
  EXPECT_EQ(2, ip_func->all_args().size());
  EXPECT_MATCH(ip_func->all_args()[0], ColumnNode("local_addr"));
  EXPECT_MATCH(ip_func->all_args()[1], ColumnNode("time_"));
  EXPECT_MATCH(ip_func, ResolvedExpression());

  // Check that the semantic type of the conversion func is propagated properly.
  auto type_or_s = map->resolved_table_type()->GetColumnType("md");
  ASSERT_OK(type_or_s);
  auto type = std::static_pointer_cast<ValueType>(type_or_s.ConsumeValueOrDie());
  EXPECT_EQ(types::STRING, type->data_type());
  EXPECT_EQ(types::ST_POD_NAME, type->semantic_type());
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
