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

#include <google/protobuf/text_format.h>
#include <memory>
#include <vector>

#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/ir/pattern_match.h"
#include "src/common/testing/testing.h"

namespace px {
namespace carnot {
namespace planner {
class PatternMatchTest : public OperatorTests {};

TEST_F(PatternMatchTest, equals_test) {
  auto c1 = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto c2 = graph->CreateNode<IntIR>(ast, 11).ValueOrDie();

  auto func = MakeEqualsFunc(c1, c2);

  EXPECT_MATCH(func, Equals(Int(10), Value()));
  EXPECT_MATCH(func, Equals(Value(), Int()));
  EXPECT_FALSE(Match(func, Equals(Value(), Int(9))));
}

// This bin op test makes sure that non_op doesn't throw errors
// while pattern matching
TEST_F(PatternMatchTest, arbitrary_bin_op_test) {
  auto c1 = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto c2 = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();

  auto func = graph
                  ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "op"},
                                       std::vector<ExpressionIR*>{c1, c2})
                  .ValueOrDie();

  EXPECT_FALSE(Match(func, Equals(Int(10), Value())));
  EXPECT_MATCH(func, BinOp(Value(), Value()));
  EXPECT_MATCH(func, BinOp());
}

TEST_F(PatternMatchTest, expression_data_type_resolution) {
  auto int1 = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto col1 = graph->CreateNode<ColumnIR>(ast, "col1", /* parent_op_idx */ 0).ValueOrDie();
  auto func = graph
                  ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "op"},
                                       std::vector<ExpressionIR*>{int1, col1})
                  .ValueOrDie();

  // Make sure expression works.
  EXPECT_MATCH(int1, Expression());
  EXPECT_MATCH(col1, Expression());
  EXPECT_MATCH(func, Expression());

  // Make sure unresolved expression works.
  EXPECT_FALSE(Match(int1, UnresolvedExpression()));
  EXPECT_MATCH(col1, UnresolvedExpression());
  EXPECT_MATCH(func, UnresolvedExpression());

  // Make sure resolved expression works.
  EXPECT_MATCH(int1, ResolvedExpression());
  EXPECT_FALSE(Match(col1, ResolvedExpression()));
  EXPECT_FALSE(Match(func, ResolvedExpression()));

  // Specific expressions
  EXPECT_MATCH(col1, UnresolvedColumnType());
  EXPECT_FALSE(Match(func, UnresolvedColumnType()));
  EXPECT_FALSE(Match(col1, UnresolvedFuncType()));
  EXPECT_MATCH(func, UnresolvedFuncType());

  // Test out UnresolvedRTFuncMatchAllArgs.
  EXPECT_FALSE(Match(func, UnresolvedRTFuncMatchAllArgs(ResolvedExpression())));

  // Resolve column and check whether test works.
  EXPECT_OK(col1->SetResolvedType(ValueType::Create(types::DataType::INT64, types::ST_NONE)));
  EXPECT_MATCH(col1, ResolvedExpression());
  EXPECT_MATCH(col1, ResolvedColumnType());

  // Should Pass now
  EXPECT_MATCH(func, UnresolvedRTFuncMatchAllArgs(ResolvedExpression()));

  // Make sure that resolution works
  EXPECT_OK(func->SetResolvedType(ValueType::Create(types::DataType::INT64, types::ST_NONE)));
  EXPECT_MATCH(func, ResolvedExpression());
  EXPECT_MATCH(func, ResolvedFuncType());
}

TEST_F(PatternMatchTest, relation_status_operator_match) {
  table_store::schema::Relation test_relation;
  test_relation.AddColumn(types::DataType::INT64, "col1");
  test_relation.AddColumn(types::DataType::INT64, "col2");
  auto mem_src =
      graph->CreateNode<MemorySourceIR>(ast, "source", std::vector<std::string>{}).ValueOrDie();
  auto blocking_agg =
      graph
          ->CreateNode<BlockingAggIR>(ast, mem_src, std::vector<ColumnIR*>{}, ColExpressionVector{})
          .ValueOrDie();
  auto map =
      graph->CreateNode<MapIR>(ast, mem_src, ColExpressionVector{}, /*keep_input_columns*/ false)
          .ValueOrDie();
  ExpressionIR* filter_expr = graph->CreateNode<BoolIR>(ast, false).ValueOrDie();
  auto filter = graph->CreateNode<FilterIR>(ast, mem_src, filter_expr).ValueOrDie();

  // Unresolved blocking aggregate with unresolved parent.
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyOp(BlockingAgg())));
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyOp(Map())));
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyOp()));
  // Unesolved map with unresolved parent.
  EXPECT_FALSE(Match(map, UnresolvedReadyOp(BlockingAgg())));
  EXPECT_FALSE(Match(map, UnresolvedReadyOp(Map())));
  EXPECT_FALSE(Match(map, UnresolvedReadyOp()));
  // Unresolved Filter with unresolved parent.
  EXPECT_FALSE(Match(filter, UnresolvedReadyOp(BlockingAgg())));
  EXPECT_FALSE(Match(filter, UnresolvedReadyOp(Map())));
  EXPECT_FALSE(Match(filter, UnresolvedReadyOp()));

  // Resolve parent.
  EXPECT_OK(mem_src->SetResolvedType(TableType::Create(test_relation)));
  // Unresolved blocking aggregate with resolved parent.
  EXPECT_MATCH(blocking_agg, UnresolvedReadyOp(BlockingAgg()));
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyOp(Map())));
  EXPECT_MATCH(blocking_agg, UnresolvedReadyOp());
  // Unresolved map with resolved parent.
  EXPECT_FALSE(Match(map, UnresolvedReadyOp(BlockingAgg())));
  EXPECT_MATCH(map, UnresolvedReadyOp(Map()));
  EXPECT_MATCH(map, UnresolvedReadyOp());
  // Unresolved Filter with resolved parent.
  EXPECT_FALSE(Match(filter, UnresolvedReadyOp(BlockingAgg())));
  EXPECT_FALSE(Match(filter, UnresolvedReadyOp(Map())));
  EXPECT_MATCH(filter, UnresolvedReadyOp());

  // Resolve children.
  EXPECT_OK(blocking_agg->SetResolvedType(TableType::Create(test_relation)));
  EXPECT_OK(map->SetResolvedType(TableType::Create(test_relation)));
  EXPECT_OK(filter->SetResolvedType(TableType::Create(test_relation)));
  // Resolved blocking aggregate with resolved parent.
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyOp(BlockingAgg())));
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyOp(Map())));
  EXPECT_FALSE(Match(blocking_agg, UnresolvedReadyOp()));
  // Resolved map with resolved parent.
  EXPECT_FALSE(Match(map, UnresolvedReadyOp(BlockingAgg())));
  EXPECT_FALSE(Match(map, UnresolvedReadyOp(Map())));
  EXPECT_FALSE(Match(map, UnresolvedReadyOp()));
  // Resolved Filter with resolved parent.
  EXPECT_FALSE(Match(filter, UnresolvedReadyOp(BlockingAgg())));
  EXPECT_FALSE(Match(filter, UnresolvedReadyOp(Map())));
  EXPECT_FALSE(Match(filter, UnresolvedReadyOp()));
}

TEST_F(PatternMatchTest, relation_status_union_test) {
  MemorySourceIR* mem_src1 = MakeMemSource();
  MemorySourceIR* mem_src2 = MakeMemSource();

  UnionIR* union_op = MakeUnion({mem_src1, mem_src2});

  EXPECT_FALSE(Match(union_op, UnresolvedReadyOp(Union())));

  EXPECT_OK(mem_src1->SetResolvedType(TableType::Create(MakeRelation())));

  // Check to make sure that one parent doesn't set it off.
  EXPECT_FALSE(Match(union_op, UnresolvedReadyOp(Union())));

  EXPECT_OK(mem_src2->SetResolvedType(TableType::Create(MakeRelation())));
  // Check to make sure that setting both parents does set it off.
  EXPECT_MATCH(union_op, UnresolvedReadyOp(Union()));
}

TEST_F(PatternMatchTest, OpWithParentMatch) {
  auto mem_src1 = MakeMemSource();
  auto group = MakeGroupBy(mem_src1, {MakeColumn("c", 0)});
  auto agg = MakeBlockingAgg(group, {}, {{"a", MakeMeanFunc(MakeColumn("b", 0))}});

  EXPECT_MATCH(agg, OperatorWithParent(BlockingAgg(), GroupBy()));
  EXPECT_MATCH(group, OperatorWithParent(GroupBy(), MemorySource()));
  EXPECT_FALSE(Match(group, OperatorWithParent(BlockingAgg(), GroupBy())));
}

}  // namespace planner

}  // namespace carnot
}  // namespace px
