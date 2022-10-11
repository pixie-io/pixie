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

#include <queue>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <pypa/ast/ast.hh>

#include <sole.hpp>
#include "src/carnot/planner/compiler/analyzer/resolve_types_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/pattern_match.h"
#include "src/carnot/planner/metadata/metadata_handler.h"
#include "src/common/testing/protobuf.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace planner {
using compiler::ResolveTypesRule;
using ::px::testing::proto::EqualsProto;
using table_store::schema::Relation;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

TEST(IRTypes, types_enum_test) {
  // Quick test to make sure the enums test is inline with the type strings.
  EXPECT_EQ(static_cast<int64_t>(IRNodeType::number_of_types),
            sizeof(kIRNodeStrings) / sizeof(*kIRNodeStrings));
}

TEST(IRTest, FindNodesOfType) {
  auto ast = MakeTestAstPtr();
  auto ig = std::make_shared<IR>();
  auto src = ig->CreateNode<MemorySourceIR>(ast, "table_str", std::vector<std::string>{"testCol"})
                 .ValueOrDie();

  auto int_nodes = ig->FindNodesOfType(IRNodeType::kMemorySource);
  EXPECT_THAT(int_nodes, UnorderedElementsAre(src));

  // Shouldn't return anything when it doesn't have a type of node.
  auto string_nodes = ig->FindNodesOfType(IRNodeType::kString);
  EXPECT_EQ(string_nodes.size(), 0);
}

TEST(IRTest, FindNodesThatMatch) {
  auto ast = MakeTestAstPtr();
  auto ig = std::make_shared<IR>();
  auto src = ig->CreateNode<MemorySourceIR>(ast, "table_str", std::vector<std::string>{"testCol"})
                 .ValueOrDie();

  auto int_nodes = ig->FindNodesThatMatch(MemorySource());
  EXPECT_THAT(int_nodes, UnorderedElementsAre(src));

  // Shouldn't return anything when it doesn't have a type of node.
  auto string_nodes = ig->FindNodesThatMatch(String());
  EXPECT_EQ(string_nodes.size(), 0);
}

TEST(IRTest, MapSharedNodes) {
  auto ast = MakeTestAstPtr();
  auto ig = std::make_shared<IR>();
  auto src =
      ig->CreateNode<MemorySourceIR>(ast, "table", std::vector<std::string>{"col"}).ValueOrDie();
  auto col_expr = ig->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto map =
      ig->CreateNode<MapIR>(ast, src, ColExpressionVector{{"col1", col_expr}, {"col2", col_expr}},
                            /* keep_input_columns */ false)
          .ValueOrDie();

  ASSERT_EQ(2, map->col_exprs().size());
  EXPECT_NE(map->col_exprs()[0].node->id(), map->col_exprs()[1].node->id());
  CompareClone(map->col_exprs()[0].node, map->col_exprs()[1].node, "Map column expression");
}

constexpr char kExpectedMemSrcPb[] = R"(
  op_type: MEMORY_SOURCE_OPERATOR
  mem_source_op {
    name: "test_table"
    column_idxs: 0
    column_idxs: 1
    column_names: "cpu0"
    column_names: "cpu1"
    column_types: INT64
    column_types: FLOAT64
    start_time: {
      value: 10
    }
    stop_time: {
      value: 20
    }
  }
)";

using ToProtoTest = ASTVisitorTest;

TEST_F(ToProtoTest, memory_source_ir) {
  auto mem_src =
      graph->CreateNode<MemorySourceIR>(ast, "test_table", std::vector<std::string>{}).ValueOrDie();
  auto relation = Relation({types::DataType::INT64, types::DataType::FLOAT64}, {"cpu0", "cpu1"});
  compiler_state_->relation_map()->emplace("test_table", relation);

  mem_src->SetColumnIndexMap({0, 1});
  mem_src->SetTimeStartNS(10);
  mem_src->SetTimeStopNS(20);

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  planpb::Operator pb;
  EXPECT_OK(mem_src->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(kExpectedMemSrcPb));
}

constexpr char kExpectedEmptySrcPb[] = R"(
  op_type: EMPTY_SOURCE_OPERATOR
  empty_source_op {
    column_names: "cpu0"
    column_names: "cpu1"
    column_types: INT64
    column_types: FLOAT64
  }
)";

TEST_F(ToProtoTest, empty_source_ir) {
  ASSERT_OK_AND_ASSIGN(
      EmptySourceIR * empty_source,
      graph->CreateNode<EmptySourceIR>(
          ast, TableType::Create(Relation{{types::DataType::INT64, types::DataType::FLOAT64},
                                          {"cpu0", "cpu1"}})));

  planpb::Operator pb;
  EXPECT_OK(empty_source->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(kExpectedEmptySrcPb));
}

constexpr char kExpectedMemSrcWithTabletPb[] = R"(
  op_type: MEMORY_SOURCE_OPERATOR
  mem_source_op {
    name: "test_table"
    column_idxs: 0
    column_idxs: 1
    column_names: "cpu0"
    column_names: "cpu1"
    column_types: INT64
    column_types: FLOAT64
    start_time: {
      value: 10
    }
    stop_time: {
      value: 20
    }

    tablet: $0
  }
)";

TEST_F(ToProtoTest, memory_source_ir_with_tablet) {
  auto mem_src =
      graph->CreateNode<MemorySourceIR>(ast, "test_table", std::vector<std::string>{"cpu0", "cpu1"})
          .ConsumeValueOrDie();

  auto rel = Relation({types::DataType::INT64, types::DataType::FLOAT64}, {"cpu0", "cpu1"});
  compiler_state_->relation_map()->emplace("test_table", rel);

  mem_src->SetTimeStartNS(10);
  mem_src->SetTimeStopNS(20);

  types::TabletID tablet_value = "abcd";

  mem_src->SetTabletValue(tablet_value);

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  planpb::Operator pb;
  EXPECT_OK(mem_src->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(absl::Substitute(kExpectedMemSrcWithTabletPb,
                                               absl::Substitute("\"$0\"", tablet_value))));
}

constexpr char kExpectedMemSinkPb[] = R"(
  op_type: MEMORY_SINK_OPERATOR
  mem_sink_op {
    name: "output_table"
    column_names: "output1"
    column_names: "output2"
    column_types: INT64
    column_types: FLOAT64
    column_semantic_types: ST_NONE
    column_semantic_types: ST_NONE
  }
)";

TEST_F(ToProtoTest, memory_sink_ir) {
  auto mem_source =
      graph->CreateNode<MemorySourceIR>(ast, "source", std::vector<std::string>{}).ValueOrDie();
  auto mem_sink = graph
                      ->CreateNode<MemorySinkIR>(ast, mem_source, "output_table",
                                                 std::vector<std::string>{"output1", "output2"})
                      .ValueOrDie();

  auto rel = table_store::schema::Relation(
      std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64}),
      std::vector<std::string>({"output1", "output2"}));
  compiler_state_->relation_map()->emplace("source", rel);

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  planpb::Operator pb;
  EXPECT_OK(mem_sink->ToProto(&pb));

  planpb::Operator expected_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kExpectedMemSinkPb, &expected_pb));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(expected_pb, pb));
}

constexpr char kExpectedMapPb[] = R"(
  op_type: MAP_OPERATOR
  map_op {
    column_names: "col_name"
    expressions {
      func {
        name: "add"
        args {
          constant {
            data_type: INT64
            int64_value: 10
          }
        }
        args {
          column {
            node: 3
            index: 4
          }
        }
        args_data_types: INT64
        args_data_types: INT64
      }
    }
  }
)";

TEST_F(ToProtoTest, map_ir) {
  auto constant = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto col = graph->CreateNode<ColumnIR>(ast, "col4", /*parent_op_idx*/ 0).ValueOrDie();
  EXPECT_OK(col->SetResolvedType(ValueType::Create(types::INT64, types::ST_NONE)));
  auto func = graph
                  ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::add, "+", "add"},
                                       std::vector<ExpressionIR*>({constant, col}))
                  .ValueOrDie();

  auto mem_src =
      graph
          ->CreateNode<MemorySourceIR>(
              ast, "table_name", std::vector<std::string>{"col0", "col1", "col2", "col3", "col4"})
          .ValueOrDie();
  table_store::schema::Relation relation(
      {types::INT64, types::INT64, types::INT64, types::INT64, types::INT64},
      {"col0", "col1", "col2", "col3", "col4"});
  compiler_state_->relation_map()->emplace("table_name", relation);
  ASSERT_OK(ResolveOperatorType(mem_src, compiler_state_.get()));
  auto map = graph
                 ->CreateNode<MapIR>(ast, mem_src, ColExpressionVector{{"col_name", func}},
                                     /* keep_input_columns */ false)
                 .ValueOrDie();
  ASSERT_OK(ResolveOperatorType(map, compiler_state_.get()));

  planpb::Operator pb;
  EXPECT_OK(map->ToProto(&pb));
  EXPECT_THAT(pb, EqualsProto(kExpectedMapPb));
}

constexpr char kExpectedFilterPb[] = R"(
  op_type: FILTER_OPERATOR
  filter_op {
    expression {
      func {
        name: "equal"
        args {
          column {
            node: 1
            index: 1
          }
        }
        args {
          column {
            node: 1
            index: 3
          }
        }
        args_data_types: FLOAT64
        args_data_types: FLOAT64
      }
    }
    columns {
      node: 1
    }
    columns {
      node: 1
      index: 1
    }
    columns {
      node: 1
      index: 2
    }
    columns {
      node: 1
      index: 3
    }
  }
)";

TEST_F(ToProtoTest, filter_to_proto) {
  // Make the mem_src not node_id 0
  MakeInt(1);
  auto rel = MakeRelation();
  auto mem_src = MakeMemSource("table", rel);
  compiler_state_->relation_map()->emplace("table", rel);
  auto col1 = MakeColumn("cpu0", 0);
  auto col2 = MakeColumn("cpu2", 0);
  auto equals = MakeEqualsFunc(col1, col2);

  auto filter = MakeFilter(mem_src, equals);

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  planpb::Operator pb;
  EXPECT_OK(filter->ToProto(&pb));
  EXPECT_THAT(pb, EqualsProto(kExpectedFilterPb)) << pb.DebugString();
}

constexpr char kExpectedAggPb[] = R"(
  op_type: AGGREGATE_OPERATOR
  agg_op {
    windowed: false
    values {
      name: "mean"
      id: 0
      args {
        constant {
          data_type: INT64
          int64_value: 10
        }
      }
      args {
        column {
          node: 0
          index: 2
        }
      }
      args_data_types: INT64
      args_data_types: INT64
    }
    groups {
      node: 0
      index: 1
    }
    group_names: "group1"
    value_names: "mean"
    partial_agg: true
    finalize_results: true
  }
)";

TEST_F(ToProtoTest, agg_ir) {
  auto mem_src = graph
                     ->CreateNode<MemorySourceIR>(
                         ast, "source", std::vector<std::string>{"col1", "group1", "column"})
                     .ValueOrDie();
  table_store::schema::Relation rel({types::INT64, types::INT64, types::INT64},
                                    {"col1", "group1", "column"});
  compiler_state_->relation_map()->emplace("source", rel);
  auto constant = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto col = graph->CreateNode<ColumnIR>(ast, "column", /*parent_op_idx*/ 0).ValueOrDie();
  EXPECT_OK(col->SetResolvedType(ValueType::Create(types::INT64, types::ST_NONE)));

  auto agg_func = graph
                      ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "mean"},
                                           std::vector<ExpressionIR*>{constant, col})
                      .ValueOrDie();
  EXPECT_OK(AddUDAToRegistry("mean", types::INT64, {types::INT64, types::INT64}));
  auto group1 = graph->CreateNode<ColumnIR>(ast, "group1", /*parent_op_idx*/ 0).ValueOrDie();
  EXPECT_OK(group1->SetResolvedType(ValueType::Create(types::INT64, types::ST_NONE)));

  auto agg = graph
                 ->CreateNode<BlockingAggIR>(ast, mem_src, std::vector<ColumnIR*>{group1},
                                             ColExpressionVector{{"mean", agg_func}})
                 .ValueOrDie();

  ASSERT_OK(ResolveOperatorType(mem_src, compiler_state_.get()));
  ASSERT_OK(ResolveOperatorType(agg, compiler_state_.get()));

  planpb::Operator pb;
  ASSERT_OK(agg->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(kExpectedAggPb));
}

TEST_F(ToProtoTest, agg_ir_with_presplit_proto) {
  auto mem_src = graph
                     ->CreateNode<MemorySourceIR>(
                         ast, "source", std::vector<std::string>{"col1", "group1", "column"})
                     .ValueOrDie();
  table_store::schema::Relation rel({types::INT64, types::INT64, types::INT64},
                                    {"col1", "group1", "column"});
  compiler_state_->relation_map()->emplace("source", rel);
  auto constant = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto col = graph->CreateNode<ColumnIR>(ast, "column", /*parent_op_idx*/ 0).ValueOrDie();
  EXPECT_OK(col->SetResolvedType(ValueType::Create(types::INT64, types::ST_NONE)));

  auto agg_func = graph
                      ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "mean"},
                                           std::vector<ExpressionIR*>{constant, col})
                      .ValueOrDie();
  EXPECT_OK(AddUDAToRegistry("mean", types::INT64, {types::INT64, types::INT64}));
  auto group1 = graph->CreateNode<ColumnIR>(ast, "group1", /*parent_op_idx*/ 0).ValueOrDie();
  EXPECT_OK(group1->SetResolvedType(ValueType::Create(types::INT64, types::ST_NONE)));

  auto agg = graph
                 ->CreateNode<BlockingAggIR>(ast, mem_src, std::vector<ColumnIR*>{group1},
                                             ColExpressionVector{{"mean", agg_func}})
                 .ValueOrDie();

  ASSERT_OK(ResolveOperatorType(mem_src, compiler_state_.get()));
  ASSERT_OK(ResolveOperatorType(agg, compiler_state_.get()));

  planpb::Operator pb;
  ASSERT_OK(agg->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(kExpectedAggPb));

  ASSERT_OK_AND_ASSIGN(BlockingAggIR * cloned_agg, graph->CopyNode(agg));
  ASSERT_OK(cloned_agg->CopyParentsFrom(agg));
  cloned_agg->SetPreSplitProto(pb.agg_op());

  cloned_agg->SetPartialAgg(false);

  planpb::Operator cloned_pb;
  ASSERT_OK(cloned_agg->ToProto(&cloned_pb));
  // Should fail without partial agg set true.
  EXPECT_THAT(cloned_pb, Not(EqualsProto(kExpectedAggPb)));
  // Quick swap of the partial_agg value.
  cloned_pb.mutable_agg_op()->set_partial_agg(true);
  EXPECT_THAT(cloned_pb, EqualsProto(kExpectedAggPb));
}

constexpr char kExpectedLimitPb[] = R"(
  op_type: LIMIT_OPERATOR
  limit_op {
    limit: 12
    columns {
      node: 0
      index: 0
    }
    columns {
      node: 0
      index: 1
    }
    columns {
      node: 0
      index: 2
    }
  }
)";

TEST_F(ToProtoTest, limit_ir) {
  auto mem_src = graph
                     ->CreateNode<MemorySourceIR>(
                         ast, "source", std::vector<std::string>{"col1", "group1", "column"})
                     .ValueOrDie();
  table_store::schema::Relation src_rel({types::INT64, types::INT64, types::INT64},
                                        {"col1", "group1", "column"});
  compiler_state_->relation_map()->emplace("source", src_rel);

  auto limit = graph->CreateNode<LimitIR>(ast, mem_src, 12).ValueOrDie();

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  planpb::Operator pb;
  ASSERT_OK(limit->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(kExpectedLimitPb));
}

constexpr char kInt64PbTxt[] = R"proto(
constant {
  data_type: INT64
  int64_value: 123
})proto";

TEST_F(ToProtoTest, int_ir) {
  auto ast = MakeTestAstPtr();
  auto graph = std::make_shared<IR>();
  auto data_ir = graph->CreateNode<IntIR>(ast, 123).ConsumeValueOrDie();

  planpb::ScalarExpression pb;

  ASSERT_OK(data_ir->ToProto(&pb));
  EXPECT_THAT(pb, EqualsProto(kInt64PbTxt));
}

constexpr char kStringPbTxt[] = R"proto(
constant {
  data_type: STRING
  string_value: "pixie"
})proto";

TEST_F(ToProtoTest, string_ir) {
  auto ast = MakeTestAstPtr();
  auto graph = std::make_shared<IR>();
  auto data_ir = graph->CreateNode<StringIR>(ast, "pixie").ConsumeValueOrDie();

  planpb::ScalarExpression pb;

  ASSERT_OK(data_ir->ToProto(&pb));
  EXPECT_THAT(pb, EqualsProto(kStringPbTxt));
}

constexpr char kFloatPbTxt[] = R"proto(
constant {
  data_type: FLOAT64
  float64_value: 1.23
})proto";

TEST_F(ToProtoTest, float_ir) {
  auto ast = MakeTestAstPtr();
  auto graph = std::make_shared<IR>();
  auto data_ir = graph->CreateNode<FloatIR>(ast, 1.23).ConsumeValueOrDie();

  planpb::ScalarExpression pb;

  ASSERT_OK(data_ir->ToProto(&pb));
  EXPECT_THAT(pb, EqualsProto(kFloatPbTxt));
}

constexpr char kUInt128PbTxt[] = R"proto(
constant {
  data_type: UINT128
  uint128_value {
    high: 123
    low: 456
  }
})proto";

TEST_F(ToProtoTest, uint128_ir) {
  auto ast = MakeTestAstPtr();
  auto graph = std::make_shared<IR>();
  auto data_ir = graph->CreateNode<UInt128IR>(ast, absl::MakeUint128(123, 456)).ConsumeValueOrDie();

  planpb::ScalarExpression pb;

  ASSERT_OK(data_ir->ToProto(&pb));
  EXPECT_THAT(pb, EqualsProto(kUInt128PbTxt));
}

constexpr char kTimePbTxt[] = R"proto(
constant {
  data_type: TIME64NS
  time64_ns_value: 123
})proto";

TEST_F(ToProtoTest, time_ir) {
  auto ast = MakeTestAstPtr();
  auto graph = std::make_shared<IR>();
  auto data_ir = graph->CreateNode<TimeIR>(ast, 123).ConsumeValueOrDie();

  planpb::ScalarExpression pb;

  ASSERT_OK(data_ir->ToProto(&pb));
  EXPECT_THAT(pb, EqualsProto(kTimePbTxt));
}

constexpr char kBoolPbTxt[] = R"proto(
constant {
  data_type: BOOLEAN
  bool_value: true
})proto";

TEST_F(ToProtoTest, bool_ir) {
  auto ast = MakeTestAstPtr();
  auto graph = std::make_shared<IR>();
  auto data_ir = graph->CreateNode<BoolIR>(ast, true).ConsumeValueOrDie();

  planpb::ScalarExpression pb;

  ASSERT_OK(data_ir->ToProto(&pb));
  EXPECT_THAT(pb, EqualsProto(kBoolPbTxt));
}

constexpr char kSimpleFuncPbTxt[] = R"proto(
func{
  name: "foobar1"
  args {
    constant {
      data_type: INT64
      int64_value: 123
    }
  }
  args {
    constant {
      data_type: INT64
      int64_value: 456
    }
  }
  args_data_types: INT64
  args_data_types: INT64
})proto";

constexpr char kNestedFuncPbTxt[] = R"proto(
func {
  id: 1
  name: "foobar2"
  args {
    constant {
      data_type: INT64
      int64_value: 789
    }
  }
  args {$0}
  args_data_types: INT64
  args_data_types: INT64
}
)proto";
constexpr char kInitArgsFuncPbTxt[] = R"proto(
func {
  name: "foobar3"
  id: 2
  init_args {
    data_type: STRING
    string_value: "abcd"
  }
  args {
    constant {
      data_type: INT64
      int64_value: 123
    }
  }
  args_data_types: INT64
}
)proto";

TEST_F(ToProtoTest, func_tests) {
  auto int1 = graph->CreateNode<IntIR>(ast, 123).ConsumeValueOrDie();
  auto int2 = graph->CreateNode<IntIR>(ast, 456).ConsumeValueOrDie();
  auto int3 = graph->CreateNode<IntIR>(ast, 789).ConsumeValueOrDie();
  auto init_string = graph->CreateNode<StringIR>(ast, "abcd").ConsumeValueOrDie();

  auto foobar1_fn = graph
                        ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "foobar1"},
                                             std::vector<ExpressionIR*>{int1, int2})
                        .ConsumeValueOrDie();
  EXPECT_OK(AddUDFToRegistry("foobar1", types::INT64, {types::INT64, types::INT64}));
  ASSERT_OK(ResolveExpressionType(foobar1_fn, compiler_state_.get(), {}));
  auto foobar2_fn = graph
                        ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "foobar2"},
                                             std::vector<ExpressionIR*>{int3, foobar1_fn})
                        .ConsumeValueOrDie();
  EXPECT_OK(AddUDFToRegistry("foobar2", types::INT64, {types::INT64, types::INT64}));
  ASSERT_OK(ResolveExpressionType(foobar2_fn, compiler_state_.get(), {}));
  ASSERT_THAT(foobar2_fn->args(), ElementsAre(int3, foobar1_fn));

  auto foobar3_fn = graph
                        ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "foobar3"},
                                             std::vector<ExpressionIR*>{init_string, int1})
                        .ConsumeValueOrDie();
  EXPECT_OK(AddUDFToRegistry("foobar3", types::STRING, {types::STRING}, {types::INT64}));
  ASSERT_OK(ResolveExpressionType(foobar3_fn, compiler_state_.get(), {}));

  planpb::ScalarExpression pb1;

  ASSERT_OK(foobar1_fn->ToProto(&pb1));
  EXPECT_THAT(pb1, EqualsProto(kSimpleFuncPbTxt)) << pb1.DebugString();

  planpb::ScalarExpression pb2;

  ASSERT_OK(foobar2_fn->ToProto(&pb2));
  EXPECT_THAT(pb2, EqualsProto(absl::Substitute(kNestedFuncPbTxt, kSimpleFuncPbTxt)))
      << pb2.DebugString();

  planpb::ScalarExpression pb3;

  ASSERT_OK(foobar3_fn->ToProto(&pb3));
  EXPECT_THAT(pb3, EqualsProto(kInitArgsFuncPbTxt)) << pb3.DebugString();
}

constexpr char kColumnPbTxt[] = R"proto(
column {
  node: $0
  index: 1
})proto";

TEST_F(ToProtoTest, column_tests) {
  auto column = graph->CreateNode<ColumnIR>(ast, "column", 0).ConsumeValueOrDie();

  auto mem_src =
      graph->CreateNode<MemorySourceIR>(ast, "source", std::vector<std::string>{"foo", "column"})
          .ConsumeValueOrDie();
  table_store::schema::Relation rel({types::INT64, types::INT64}, {"foo", "column"});
  compiler_state_->relation_map()->emplace("source", rel);
  graph->CreateNode<MapIR>(ast, mem_src, ColExpressionVector{{"column", column}}, false)
      .ConsumeValueOrDie();

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  planpb::ScalarExpression pb;
  ASSERT_OK(column->ToProto(&pb));
  EXPECT_THAT(pb, EqualsProto(absl::Substitute(kColumnPbTxt, mem_src->id())));
}

class MetadataTests : public ::testing::Test {
 protected:
  void SetUp() override {
    ast = MakeTestAstPtr();
    graph = std::make_shared<IR>();
    md_handler = MetadataHandler::Create();
  }
  MemorySourceIR* MakeMemSource() {
    return graph->CreateNode<MemorySourceIR>(ast, "source", std::vector<std::string>{})
        .ValueOrDie();
  }
  pypa::AstPtr ast;
  std::shared_ptr<IR> graph;
  std::unique_ptr<MetadataHandler> md_handler;
};

TEST_F(MetadataTests, metadata_ir) {
  MetadataIR* metadata_ir =
      graph->CreateNode<MetadataIR>(ast, "pod_name", /*parent_op_idx*/ 0).ValueOrDie();
  EXPECT_TRUE(metadata_ir->IsColumn());
  EXPECT_EQ(metadata_ir->name(), "pod_name");
  EXPECT_FALSE(metadata_ir->has_property());
  auto property = std::make_unique<NameMetadataProperty>(
      MetadataType::POD_NAME, std::vector<MetadataType>({MetadataType::POD_ID}));
  metadata_ir->set_property(property.get());
  EXPECT_TRUE(metadata_ir->has_property());
}

using OpTests = ToProtoTest;
// Swapping a parent should make sure that all columns are passed over correclt.
TEST_F(OpTests, swap_parent) {
  MemorySourceIR* mem_source = MakeMemSource();
  ColumnIR* col1 = MakeColumn("test1", /*parent_op_idx*/ 0);
  ColumnIR* col2 = MakeColumn("test2", /*parent_op_idx*/ 0);
  ColumnIR* col3 = MakeColumn("test3", /*parent_op_idx*/ 0);
  FuncIR* add_func = MakeAddFunc(col3, MakeInt(3));
  MapIR* child_map =
      MakeMap(mem_source, {{{"out11", col1}, {"out2", col2}, {"out3", add_func}}, {}});
  EXPECT_EQ(col1->ReferenceID().ConsumeValueOrDie(), mem_source->id());
  EXPECT_EQ(col2->ReferenceID().ConsumeValueOrDie(), mem_source->id());
  EXPECT_EQ(col3->ReferenceID().ConsumeValueOrDie(), mem_source->id());

  // Insert a map as if we are copying from the parent. These columns are distinact from col1-3.
  MapIR* parent_map = MakeMap(mem_source, {{{"test1", MakeColumn("test1", /*parent_op_idx*/ 0)},
                                            {"test2", MakeColumn("test2", /*parent_op_idx*/ 0)},
                                            {"test3", MakeColumn("test3", /*parent_op_idx*/ 0)}},
                                           {}});

  EXPECT_NE(parent_map->id(), child_map->id());  // Sanity check.
  // Now swap the parent, and expect the children to point to the new parent.
  EXPECT_OK(child_map->ReplaceParent(mem_source, parent_map));
  EXPECT_EQ(col1->ReferenceID().ConsumeValueOrDie(), parent_map->id());
  EXPECT_EQ(col2->ReferenceID().ConsumeValueOrDie(), parent_map->id());
  EXPECT_EQ(col3->ReferenceID().ConsumeValueOrDie(), parent_map->id());
}

TEST_F(OpTests, internal_grpc_ops) {
  int64_t grpc_id = 123;
  std::string source_grpc_address = "1111";
  std::string sink_physical_id = "agent-xyz";

  MemorySourceIR* mem_src = MakeMemSource();
  GRPCSinkIR* grpc_sink = MakeGRPCSink(mem_src, grpc_id);

  std::shared_ptr<IR> new_graph = std::make_shared<IR>();

  // swaps the graph being built and returns the old_graph
  std::shared_ptr<IR> old_graph = SwapGraphBeingBuilt(new_graph);

  GRPCSourceGroupIR* grpc_src_group =
      MakeGRPCSourceGroup(grpc_id, TableType::Create(MakeRelation()));
  MakeMemSink(grpc_src_group, "out");

  grpc_src_group->SetGRPCAddress(source_grpc_address);
  EXPECT_TRUE(grpc_sink->has_destination_id());
  EXPECT_FALSE(grpc_sink->has_output_table());
  EXPECT_EQ(grpc_sink->destination_id(), grpc_id);
  EXPECT_OK(grpc_src_group->AddGRPCSink(grpc_sink, {0}));
  EXPECT_EQ(grpc_src_group->source_id(), grpc_id);
}

TEST_F(OpTests, external_grpc) {
  MemorySourceIR* mem_src = MakeMemSource();
  GRPCSinkIR* grpc_sink = MakeGRPCSink(mem_src, "output_table", std::vector<std::string>{"outcol"});
  EXPECT_FALSE(grpc_sink->has_destination_id());
  EXPECT_TRUE(grpc_sink->has_output_table());
  EXPECT_EQ("output_table", grpc_sink->name());
  EXPECT_THAT(grpc_sink->out_columns(), ElementsAre("outcol"));
}

using CloneTests = ToProtoTest;
TEST_F(CloneTests, simple_clone) {
  Relation rel({types::INT64, types::INT64, types::INT64}, {"test1", "test2", "test3"});
  auto mem_source = MakeMemSource("source", rel);
  compiler_state_->relation_map()->emplace("source", rel);
  ColumnIR* col1 = MakeColumn("test1", 0);
  ColumnIR* col2 = MakeColumn("test2", 0);
  col2->set_annotations(ExpressionIR::Annotations(MetadataType::POD_NAME));
  ColumnIR* col3 = MakeColumn("test3", 0);
  FuncIR* add_func = MakeAddFunc(col3, MakeInt(3));
  MapIR* map = MakeMap(mem_source, {{{"out1", col1}, {"out2", col2}, {"out3", add_func}}, {}});
  MakeMemSink(map, "out");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto out = graph->Clone();
  EXPECT_OK(out.status());
  std::unique_ptr<IR> cloned_ir = out.ConsumeValueOrDie();

  ASSERT_EQ(graph->dag().TopologicalSort(), cloned_ir->dag().TopologicalSort());

  // Make sure that all of the columns are now part of the new graph.
  for (int64_t i : cloned_ir->dag().TopologicalSort()) {
    CompareClone(cloned_ir->Get(i), graph->Get(i), absl::Substitute("For index $0", i));
  }
}

TEST_F(CloneTests, repeated_exprs_clone) {
  auto mem_source = MakeMemSource("table", Relation());
  compiler_state_->relation_map()->emplace("table", Relation());
  IntIR* intnode = MakeInt(105);
  FuncIR* add_func = MakeAddFunc(intnode, MakeInt(3));
  FuncIR* add_func2 = MakeAddFunc(intnode, add_func);
  MapIR* map1 = MakeMap(mem_source, {{{"int", intnode}, {"add", add_func}}});
  MapIR* map2 = MakeMap(map1, {{{"add1", add_func}, {"add2", add_func2}}});
  MakeMemSink(map2, "out");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto out = graph->Clone();
  EXPECT_OK(out.status());
  std::unique_ptr<IR> cloned_ir = out.ConsumeValueOrDie();

  ASSERT_EQ(graph->dag().TopologicalSort(), cloned_ir->dag().TopologicalSort());

  // Make sure that all of the columns are now part of the new graph.
  for (int64_t i : cloned_ir->dag().TopologicalSort()) {
    CompareClone(cloned_ir->Get(i), graph->Get(i), absl::Substitute("For index $0", i));
  }
}

TEST_F(CloneTests, all_op_clone) {
  auto mem_source = MakeMemSource("http_events");
  mem_source->set_streaming(true);

  auto property1 = std::make_unique<NameMetadataProperty>(
      MetadataType::SERVICE_NAME, std::vector<MetadataType>({MetadataType::UPID}));
  auto metadata1 = MakeMetadataIR("service", 0);
  metadata1->set_property(property1.get());
  auto equals_fn = MakeEqualsFunc(metadata1, MakeString("pl/test_service"));
  auto eq_map = MakeMap(mem_source, {{{"equals_column", equals_fn}}});
  auto filter = MakeFilter(eq_map, MakeColumn("equals_column", 0));
  auto limit = MakeLimit(filter, 10);

  auto mean_func = MakeMeanFunc(MakeColumn("equals_column", 0));
  auto property2 = std::make_unique<NameMetadataProperty>(
      MetadataType::SERVICE_NAME, std::vector<MetadataType>({MetadataType::UPID}));
  auto metadata2 = MakeMetadataIR("service", 0);
  metadata2->set_property(property2.get());
  auto agg = MakeBlockingAgg(limit, {metadata2}, {{{"mean", mean_func}}, {}});
  auto add_func = MakeAddFunc(MakeColumn("mean", 0), MakeInt(3));
  auto map = MakeMap(agg, {{{"mean_deux", add_func}, {"mean", MakeColumn("mean", 0)}}, {}});
  MakeMemSink(map, "sup");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto out = graph->Clone();
  EXPECT_OK(out.status());
  std::unique_ptr<IR> cloned_ir = out.ConsumeValueOrDie();

  ASSERT_EQ(graph->dag().TopologicalSort(), cloned_ir->dag().TopologicalSort());

  // Make sure that all of the columns are now part of the new graph.
  for (int64_t i : cloned_ir->dag().TopologicalSort()) {
    CompareClone(cloned_ir->Get(i), graph->Get(i), absl::Substitute("For index $0", i));
  }
}

TEST_F(CloneTests, grpc_source_group) {
  auto grpc_source = MakeGRPCSourceGroup(123, TableType::Create(MakeRelation()));
  grpc_source->SetGRPCAddress("1111");

  auto out = graph->Clone();
  EXPECT_OK(out.status());
  std::unique_ptr<IR> cloned_ir = out.ConsumeValueOrDie();

  ASSERT_EQ(graph->dag().TopologicalSort(), cloned_ir->dag().TopologicalSort());

  // Make sure that all of the columns are now part of the new graph.
  for (int64_t i : cloned_ir->dag().TopologicalSort()) {
    CompareClone(cloned_ir->Get(i), graph->Get(i), absl::Substitute("For index $0", i));
  }
}

TEST_F(CloneTests, internal_grpc_sink) {
  auto mem_source = MakeMemSource("source");
  compiler_state_->relation_map()->emplace("source", MakeRelation());
  GRPCSinkIR* grpc_sink = MakeGRPCSink(mem_source, 123);
  grpc_sink->SetDestinationAddress("1111");
  grpc_sink->SetDestinationSSLTargetName("kelvin.pl.svc");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto out = graph->Clone();
  EXPECT_OK(out.status());
  std::unique_ptr<IR> cloned_ir = out.ConsumeValueOrDie();

  ASSERT_EQ(graph->dag().TopologicalSort(), cloned_ir->dag().TopologicalSort());

  // Make sure that all of the columns are now part of the new graph.
  for (int64_t i : cloned_ir->dag().TopologicalSort()) {
    CompareClone(cloned_ir->Get(i), graph->Get(i), absl::Substitute("For index $0", i));
  }
}

TEST_F(CloneTests, external_grpc_sink) {
  auto mem_source = MakeMemSource("source");
  compiler_state_->relation_map()->emplace("source", MakeRelation());
  GRPCSinkIR* grpc_sink =
      MakeGRPCSink(mem_source, "output_table", std::vector<std::string>{"count"});
  grpc_sink->SetDestinationAddress("1111");
  grpc_sink->SetDestinationSSLTargetName("vizier-query-broker-svc.pl.svc");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto out = graph->Clone();
  EXPECT_OK(out.status());
  std::unique_ptr<IR> cloned_ir = out.ConsumeValueOrDie();

  ASSERT_EQ(graph->dag().TopologicalSort(), cloned_ir->dag().TopologicalSort());

  // Make sure that all of the columns are now part of the new graph.
  for (int64_t i : cloned_ir->dag().TopologicalSort()) {
    CompareClone(cloned_ir->Get(i), graph->Get(i), absl::Substitute("For index $0", i));
  }
}

TEST_F(CloneTests, grpc_source) {
  auto grpc_source = MakeGRPCSource(TableType::Create(MakeRelation()));
  MakeMemSink(grpc_source, "sup");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto out = graph->Clone();
  EXPECT_OK(out.status());
  std::unique_ptr<IR> cloned_ir = out.ConsumeValueOrDie();

  ASSERT_EQ(graph->dag().TopologicalSort(), cloned_ir->dag().TopologicalSort());

  // Make sure that all of the columns are now part of the new graph.
  for (int64_t i : cloned_ir->dag().TopologicalSort()) {
    CompareClone(cloned_ir->Get(i), graph->Get(i), absl::Substitute("For index $0", i));
  }
}

TEST_F(CloneTests, join_clone) {
  Relation relation0({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64},
                     {"left_only", "col1", "col2", "col3"});
  auto mem_src1 = MakeMemSource("source0", relation0);
  compiler_state_->relation_map()->emplace("source0", relation0);

  Relation relation1({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64, types::DataType::INT64},
                     {"right_only", "col1", "col2", "col3", "col4"});
  auto mem_src2 = MakeMemSource("source1", relation1);
  compiler_state_->relation_map()->emplace("source1", relation1);

  auto join_op = MakeJoin({mem_src1, mem_src2}, "inner", relation0, relation1,
                          std::vector<std::string>{"col1", "col3"},
                          std::vector<std::string>{"col2", "col4"}, {"left", "right"});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<IR> cloned_ir, graph->Clone());

  EXPECT_NE(graph->Get(join_op->id()), nullptr);
  IRNode* maybe_join_clone = cloned_ir->Get(join_op->id());
  ASSERT_MATCH(maybe_join_clone, Join());
  JoinIR* join_clone = static_cast<JoinIR*>(maybe_join_clone);

  CompareClone(join_clone, join_op, "");
}

TEST_F(CloneTests, union_clone) {
  compiler_state_->relation_map()->emplace("table", MakeRelation());
  auto mem_src1 = MakeMemSource(MakeRelation());
  auto mem_src2 = MakeMemSource(MakeRelation());

  auto union_op = MakeUnion({mem_src1, mem_src2});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto out = graph->Clone();

  EXPECT_OK(out.status());
  std::unique_ptr<IR> cloned_ir = out.ConsumeValueOrDie();

  // Check the memory sources.
  for (const auto& mem : cloned_ir->FindNodesThatMatch(MemorySource())) {
    auto mem_src_child = static_cast<MemorySourceIR*>(mem)->Children()[0];
    EXPECT_MATCH(mem_src_child, Union());
    EXPECT_EQ(mem_src_child->id(), union_op->id());
  }

  graph->Get(union_op->id());
  IRNode* maybe_union_clone = cloned_ir->Get(union_op->id());
  ASSERT_EQ(maybe_union_clone->type(), IRNodeType::kUnion);
  UnionIR* union_clone = static_cast<UnionIR*>(maybe_union_clone);

  CompareClone(union_clone, union_op, "");
}

TEST_F(CloneTests, copy_into_existing_dag) {
  auto src = MakeMemSource();
  compiler_state_->relation_map()->emplace("table", Relation());
  IntIR* intnode = MakeInt(105);
  FuncIR* add_func = MakeAddFunc(intnode, MakeInt(3));
  FuncIR* add_func2 = MakeAddFunc(intnode, add_func);
  ASSERT_OK(ResolveExpressionType(add_func, compiler_state_.get(), {}));
  ASSERT_OK(ResolveExpressionType(add_func2, compiler_state_.get(), {}));
  MapIR* map = MakeMap(src, {{{"int", intnode}, {"add1", add_func}, {"add2", add_func2}}});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto source_out = graph->CopyNode(src);
  EXPECT_OK(source_out.status());
  auto map_out = graph->CopyNode(map);
  EXPECT_OK(source_out.status());
  OperatorIR* cmap = map_out.ConsumeValueOrDie();
  OperatorIR* csrc = source_out.ConsumeValueOrDie();
  EXPECT_OK(cmap->AddParent(csrc));

  std::queue<int64_t> original_q;
  std::queue<int64_t> clone_q;
  original_q.push(src->id());
  clone_q.push(csrc->id());

  int64_t subtree_node_count = 0;
  while (original_q.size()) {
    EXPECT_EQ(original_q.size(), clone_q.size());
    auto original_id = original_q.front();
    auto clone_id = clone_q.front();
    original_q.pop();
    clone_q.pop();
    CompareClone(graph->Get(original_id), graph->Get(clone_id),
                 absl::Substitute("For index $0", original_id));
    for (auto original_child_id : graph->dag().DependenciesOf(original_id)) {
      original_q.push(original_child_id);
    }
    for (auto clone_child_id : graph->dag().DependenciesOf(clone_id)) {
      clone_q.push(clone_child_id);
    }
    subtree_node_count++;
  }
  CHECK_EQ(subtree_node_count, 11);
}

TEST_F(CloneTests, repeated_exprs_copy_selected_nodes) {
  auto mem_source = MakeMemSource();
  compiler_state_->relation_map()->emplace("table", Relation());
  IntIR* intnode = MakeInt(105);
  FuncIR* add_func = MakeAddFunc(intnode, MakeInt(3));
  FuncIR* add_func2 = MakeAddFunc(intnode, add_func);
  ASSERT_OK(ResolveExpressionType(add_func, compiler_state_.get(), {}));
  ASSERT_OK(ResolveExpressionType(add_func2, compiler_state_.get(), {}));
  MapIR* map1 = MakeMap(mem_source, {{{"int", intnode}, {"add", add_func}}});
  MapIR* map2 = MakeMap(map1, {{{"add1", add_func}, {"add2", add_func2}}});
  auto sink = MakeMemSink(map2, "out");

  // non-copied ops.
  auto other_source = MakeMemSource();
  MakeMemSink(other_source, "not copied");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto dest = std::make_unique<IR>();
  EXPECT_OK(dest->CopyOperatorSubgraph(graph.get(), {mem_source, map1, map2, sink}));
  ASSERT_EQ(8, dest->dag().nodes().size());

  // Make sure that all of the columns are now part of the new graph.
  for (int64_t i : dest->dag().nodes()) {
    CompareClone(dest->Get(i), graph->Get(i), absl::Substitute("For index $0", i));
  }
}

using ToProtoTests = ASTVisitorTest;
constexpr char kExpectedGRPCSourcePb[] = R"proto(
  op_type: GRPC_SOURCE_OPERATOR
  grpc_source_op {
    column_types: INT64
    column_types: FLOAT64
    column_types: FLOAT64
    column_types: FLOAT64
    column_names: "count"
    column_names: "cpu0"
    column_names: "cpu1"
    column_names: "cpu2"
  }
)proto";

TEST_F(ToProtoTests, grpc_source_ir) {
  auto grpc_src = MakeGRPCSource(TableType::Create(MakeRelation()));
  MakeMemSink(grpc_src, "sink");

  planpb::Operator pb;
  ASSERT_OK(grpc_src->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(kExpectedGRPCSourcePb));
}

constexpr char kExpectedInternalGRPCSinkPb[] = R"proto(
  op_type: GRPC_SINK_OPERATOR
  grpc_sink_op {
    address: "$0"
    grpc_source_id: $1
    connection_options {
      ssl_targetname: "$2"
    }
  }
)proto";

TEST_F(ToProtoTests, internal_grpc_sink_ir) {
  int64_t destination_id = 123;
  std::string grpc_address = "1111";
  std::string ssl_targetname = "kelvin.pl.svc";
  auto mem_src = MakeMemSource();
  auto grpc_sink = MakeGRPCSink(mem_src, destination_id);
  grpc_sink->SetDestinationAddress(grpc_address);
  grpc_sink->SetDestinationSSLTargetName(ssl_targetname);
  int64_t agent_id = 0;
  grpc_sink->AddDestinationIDMap(destination_id + 1, agent_id);

  planpb::Operator pb;
  ASSERT_OK(grpc_sink->ToProto(&pb, agent_id));

  EXPECT_THAT(pb, EqualsProto(absl::Substitute(kExpectedInternalGRPCSinkPb, grpc_address,
                                               destination_id + 1, ssl_targetname)));
}

constexpr char kExpectedExternalGRPCSinkPb[] = R"proto(
  op_type: GRPC_SINK_OPERATOR
  grpc_sink_op {
    address: "$0"
    output_table {
      table_name: "$1"
      column_names: "count"
      column_names: "cpu0"
      column_names: "cpu1"
      column_names: "cpu2"
      column_types: INT64
      column_types: FLOAT64
      column_types: FLOAT64
      column_types: FLOAT64
      column_semantic_types: ST_NONE
      column_semantic_types: ST_PERCENT
      column_semantic_types: ST_PERCENT
      column_semantic_types: ST_PERCENT
    }
    connection_options {
      ssl_targetname: "$2"
    }
  }
)proto";

TEST_F(ToProtoTests, external_grpc_sink_ir) {
  std::string grpc_address = "1111";
  std::string ssl_targetname = "kelvin.pl.svc";
  auto mem_src = MakeMemSource();
  GRPCSinkIR* grpc_sink = MakeGRPCSink(mem_src, "output_table", std::vector<std::string>{});

  auto new_table = TableType::Create();
  new_table->AddColumn("count", ValueType::Create(types::INT64, types::ST_NONE));
  new_table->AddColumn("cpu0", ValueType::Create(types::FLOAT64, types::ST_PERCENT));
  new_table->AddColumn("cpu1", ValueType::Create(types::FLOAT64, types::ST_PERCENT));
  new_table->AddColumn("cpu2", ValueType::Create(types::FLOAT64, types::ST_PERCENT));

  ASSERT_OK(grpc_sink->SetResolvedType(new_table));

  grpc_sink->SetDestinationAddress(grpc_address);
  grpc_sink->SetDestinationSSLTargetName(ssl_targetname);

  planpb::Operator pb;
  ASSERT_OK(grpc_sink->ToProto(&pb));
  EXPECT_THAT(pb, EqualsProto(absl::Substitute(kExpectedExternalGRPCSinkPb, grpc_address,
                                               "output_table", ssl_targetname)));
}

constexpr char kIRProto[] = R"proto(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 0
      sorted_children: 1
    }
    nodes {
      id: 1
      sorted_parents: 0
    }
  }
  nodes {
    id: 0
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "table"
        column_idxs: 0
        column_idxs: 1
        column_idxs: 2
        column_idxs: 3
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_types: INT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: FLOAT64
      }
    }
  }
  nodes {
    id: 1
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "out"
        column_types: INT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_types: FLOAT64
        column_names: "count"
        column_names: "cpu0"
        column_names: "cpu1"
        column_names: "cpu2"
        column_semantic_types: ST_NONE
        column_semantic_types: ST_NONE
        column_semantic_types: ST_NONE
        column_semantic_types: ST_NONE
      }
    }
  }
}
)proto";
TEST_F(ToProtoTests, ir) {
  auto mem_src = MakeMemSource(MakeRelation());
  compiler_state_->relation_map()->emplace("table", MakeRelation());
  MakeMemSink(mem_src, "out");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  planpb::Plan pb = graph->ToProto().ConsumeValueOrDie();

  EXPECT_THAT(pb, EqualsProto(kIRProto));
}

constexpr char kExpectedUnionOpPb[] = R"proto(
op_type: UNION_OPERATOR
union_op {
  column_names: "count"
  column_names: "cpu0"
  column_names: "cpu1"
  column_names: "cpu2"
  column_mappings {
    column_indexes: 0
    column_indexes: 1
    column_indexes: 2
    column_indexes: 3
  }
  column_mappings {
    column_indexes: 3
    column_indexes: 2
    column_indexes: 1
    column_indexes: 0
  }
}
)proto";
TEST_F(ToProtoTests, UnionNoTime) {
  Relation relation = MakeRelation();
  auto column_names = relation.col_names();
  auto column_types = relation.col_types();
  std::reverse(std::begin(column_names), std::end(column_names));
  std::reverse(std::begin(column_types), std::end(column_types));
  Relation relation2 = Relation(column_types, column_names);

  auto mem_src1 = MakeMemSource("source1", relation);
  compiler_state_->relation_map()->emplace("source1", relation);
  auto mem_src2 = MakeMemSource("source2", relation2);
  compiler_state_->relation_map()->emplace("source2", relation2);
  auto union_op = MakeUnion({mem_src1, mem_src2});

  EXPECT_FALSE(union_op->HasColumnMappings());
  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));
  EXPECT_TRUE(union_op->HasColumnMappings());

  planpb::Operator pb;
  EXPECT_OK(union_op->ToProto(&pb));
  EXPECT_THAT(pb, EqualsProto(kExpectedUnionOpPb));
}

constexpr char kExpectedUnionOpTimePb[] = R"proto(
op_type: UNION_OPERATOR
union_op {
  column_names: "time_"
  column_names: "col1"
  column_mappings {
    column_indexes: 0
    column_indexes: 1
  }
  column_mappings {
    column_indexes: 1
    column_indexes: 0
  }
}
)proto";

TEST_F(ToProtoTests, UnionHasTime) {
  std::vector<std::string> column_names = {"time_", "col1"};
  std::vector<types::DataType> column_types = {types::DataType::TIME64NS, types::DataType::INT64};
  Relation relation(column_types, column_names);
  auto mem_src1 = MakeMemSource("source1", relation);
  compiler_state_->relation_map()->emplace("source1", relation);
  std::reverse(std::begin(column_names), std::end(column_names));
  std::reverse(std::begin(column_types), std::end(column_types));
  Relation relation2(column_types, column_names);
  auto mem_src2 = MakeMemSource("source2", relation2);
  compiler_state_->relation_map()->emplace("source2", relation2);
  auto union_op = MakeUnion({mem_src1, mem_src2});

  EXPECT_FALSE(union_op->HasColumnMappings());
  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));
  EXPECT_TRUE(union_op->HasColumnMappings());

  planpb::Operator pb;
  EXPECT_OK(union_op->ToProto(&pb));
  EXPECT_THAT(pb, EqualsProto(kExpectedUnionOpTimePb));
}

constexpr char kExpectedJoinOpPb[] = R"proto(
op_type: JOIN_OPERATOR
join_op {
  type: $0
  equality_conditions {
    left_column_index: 1
    right_column_index: 2
  }
  equality_conditions {
    left_column_index: 3
    right_column_index: 4
  }
  output_columns {
    parent_index: 0
    column_index: 0
  }
  output_columns {
    parent_index: 0
    column_index: 1
  }
  output_columns {
    parent_index: 0
    column_index: 2
  }
  output_columns {
    parent_index: 0
    column_index: 3
  }
  output_columns {
    parent_index: 1
    column_index: 0
  }
  output_columns {
    parent_index: 1
    column_index: 1
  }
  output_columns {
    parent_index: 1
    column_index: 2
  }
  output_columns {
    parent_index: 1
    column_index: 3
  }
  output_columns {
    parent_index: 1
    column_index: 4
  }
  column_names: "left_only"
  column_names: "col1"
  column_names: "col2"
  column_names: "col3"
  column_names: "right_only"
  column_names: "col1_right"
  column_names: "col2_right"
  column_names: "col3_right"
  column_names: "col4"
}
)proto";

TEST_F(ToProtoTests, inner_join) {
  Relation relation0({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64},
                     {"left_only", "col1", "col2", "col3"});
  auto mem_src1 = MakeMemSource("source0", relation0);
  compiler_state_->relation_map()->emplace("source0", relation0);

  Relation relation1({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64, types::DataType::INT64},
                     {"right_only", "col1", "col2", "col3", "col4"});
  auto mem_src2 = MakeMemSource("source1", relation1);
  compiler_state_->relation_map()->emplace("source1", relation1);

  auto join_op = MakeJoin({mem_src1, mem_src2}, "inner", relation0, relation1,
                          std::vector<std::string>{"col1", "col3"},
                          std::vector<std::string>{"col2", "col4"}, {"", "_right"});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  planpb::Operator pb;
  EXPECT_OK(join_op->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(absl::Substitute(kExpectedJoinOpPb, "INNER")));
}

TEST_F(ToProtoTests, left_join) {
  Relation relation0({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64},
                     {"left_only", "col1", "col2", "col3"});
  auto mem_src1 = MakeMemSource("source0", relation0);
  compiler_state_->relation_map()->emplace("source0", relation0);

  Relation relation1({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64, types::DataType::INT64},
                     {"right_only", "col1", "col2", "col3", "col4"});
  auto mem_src2 = MakeMemSource("source1", relation1);
  compiler_state_->relation_map()->emplace("source1", relation1);

  auto join_op = MakeJoin({mem_src1, mem_src2}, "left", relation0, relation1,
                          std::vector<std::string>{"col1", "col3"},
                          std::vector<std::string>{"col2", "col4"}, {"", "_right"});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  planpb::Operator pb;
  EXPECT_OK(join_op->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(absl::Substitute(kExpectedJoinOpPb, "LEFT_OUTER")));
}

TEST_F(ToProtoTests, full_outer) {
  Relation relation0({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64},
                     {"left_only", "col1", "col2", "col3"});
  auto mem_src1 = MakeMemSource("source0", relation0);
  compiler_state_->relation_map()->emplace("source0", relation0);

  Relation relation1({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64, types::DataType::INT64},
                     {"right_only", "col1", "col2", "col3", "col4"});
  auto mem_src2 = MakeMemSource("source1", relation1);
  compiler_state_->relation_map()->emplace("source1", relation1);

  auto join_op = MakeJoin({mem_src1, mem_src2}, "outer", relation0, relation1,
                          std::vector<std::string>{"col1", "col3"},
                          std::vector<std::string>{"col2", "col4"}, {"", "_right"});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  planpb::Operator pb;
  EXPECT_OK(join_op->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(absl::Substitute(kExpectedJoinOpPb, "FULL_OUTER")));
}

TEST_F(ToProtoTests, join_wrong_join_type) {
  Relation relation0({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64},
                     {"left_only", "col1", "col2", "col3"});
  auto mem_src1 = MakeMemSource(relation0);

  Relation relation1({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64, types::DataType::INT64},
                     {"right_only", "col1", "col2", "col3", "col4"});
  auto mem_src2 = MakeMemSource(relation1);

  std::string join_type_name = "bad_join_type";

  auto join_status = graph->CreateNode<JoinIR>(
      ast, std::vector<OperatorIR*>{mem_src1, mem_src2}, join_type_name,
      std::vector<ColumnIR*>{MakeColumn("col1", 0, relation0)},
      std::vector<ColumnIR*>{MakeColumn("col2", 1, relation1)}, std::vector<std::string>{"", ""});

  EXPECT_THAT(join_status.status(), HasCompilerError("'$0' join type not supported. Only .* "
                                                     "are available",
                                                     join_type_name));
}

TEST_F(OpTests, op_children) {
  auto mem_source = MakeMemSource();
  ColumnIR* col1 = MakeColumn("test1", 0);
  ColumnIR* col2 = MakeColumn("test2", 0);
  ColumnIR* col3 = MakeColumn("test3", 0);
  FuncIR* add_func = MakeAddFunc(col3, MakeInt(3));
  MapIR* map = MakeMap(mem_source, {{{"out1", col1}, {"out2", col2}, {"out3", add_func}}, {}});
  auto mem_sink = MakeMemSink(map, "out");

  auto mem_source_children = mem_source->Children();
  ASSERT_EQ(mem_source_children.size(), 1UL);
  EXPECT_EQ(mem_source_children[0], map);

  auto map_children = map->Children();
  ASSERT_EQ(map_children.size(), 1);
  EXPECT_EQ(map_children[0], mem_sink);

  auto mem_sink_children = mem_sink->Children();
  EXPECT_EQ(mem_sink_children.size(), 0UL);
}

using IRPruneTests = OpTests;
TEST_F(IRPruneTests, prune_test) {
  auto mem_source = MakeMemSource();
  ColumnIR* col1 = MakeColumn("test1", 0);
  ColumnIR* col2 = MakeColumn("test2", 0);
  ColumnIR* col3 = MakeColumn("test3", 0);
  FuncIR* add_func = MakeAddFunc(col3, MakeInt(3));
  MapIR* map = MakeMap(mem_source, {{{"out1", col1}, {"out2", col2}, {"out3", add_func}}, {}});
  auto mem_sink = MakeMemSink(map, "out");

  EXPECT_THAT(graph->dag().nodes(), UnorderedElementsAre(0, 1, 2, 3, 4, 5, 6, 7));
  EXPECT_THAT(graph->dag().DependenciesOf(mem_source->id()), IsSupersetOf({map->id()}));
  EXPECT_THAT(graph->dag().ParentsOf(add_func->id()), IsSupersetOf({map->id()}));

  EXPECT_OK(graph->Prune({mem_sink->id(), map->id()}));
  EXPECT_THAT(graph->dag().nodes(), UnorderedElementsAre(0, 1, 2, 3, 4, 5));
  EXPECT_THAT(graph->dag().DependenciesOf(mem_source->id()), IsEmpty());
  EXPECT_THAT(graph->dag().ParentsOf(add_func->id()), IsEmpty());
}

TEST_F(OpTests, tablet_source_group) {
  auto mem_source = MakeMemSource("table", MakeRelation());
  ASSERT_OK(mem_source->SetResolvedType(TableType::Create(MakeRelation())));
  std::vector<types::TabletID> tablet_values = {"tablet1", "tablet2"};
  auto tablet_source =
      graph->CreateNode<TabletSourceGroupIR>(ast, mem_source, tablet_values, "cpu0")
          .ConsumeValueOrDie();

  EXPECT_THAT(tablet_source->tablets(), ElementsAreArray(tablet_values));
  EXPECT_EQ(tablet_source->ReplacedMemorySource(), mem_source);
}

TEST_F(OpTests, GroupByNode) {
  auto mem_source = MakeMemSource();
  auto groupby =
      graph
          ->CreateNode<GroupByIR>(
              ast, mem_source, std::vector<ColumnIR*>{MakeColumn("col1", 0), MakeColumn("col2", 0)})
          .ValueOrDie();
  std::vector<ColumnIR*> groups = groupby->groups();
  std::vector<std::string> col_names;
  for (auto g : groups) {
    col_names.push_back(g->col_name());
  }
  EXPECT_THAT(col_names, ElementsAre("col1", "col2"));
}

TEST_F(OpTests, join_node) {
  Relation relation0({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64},
                     {"left_only", "col1", "col2", "col3"});
  auto mem_src1 = MakeMemSource(relation0);

  Relation relation1({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64, types::DataType::INT64},
                     {"right_only", "col1", "col2", "col3", "col4"});
  auto mem_src2 = MakeMemSource(relation1);

  EXPECT_OK(graph
                ->CreateNode<JoinIR>(ast, std::vector<OperatorIR*>{mem_src1, mem_src2}, "inner",
                                     std::vector<ColumnIR*>{MakeColumn("col1", 0)},
                                     std::vector<ColumnIR*>{MakeColumn("col1", 1)},
                                     std::vector<std::string>{"_x", "_y"})
                .status());
}

TEST_F(OpTests, join_node_mismatched_columns) {
  auto mem_source1 = MakeMemSource();
  auto mem_source2 = MakeMemSource();

  EXPECT_NOT_OK(graph->CreateNode<JoinIR>(
      ast, std::vector<OperatorIR*>{mem_source1, mem_source2}, "inner",
      std::vector<ColumnIR*>{MakeColumn("col1", 0), MakeColumn("col2", 0)},
      std::vector<ColumnIR*>{MakeColumn("col1", 1)}, std::vector<std::string>{}));
}

TEST_F(OpTests, join_duplicate_parents) {
  Relation relation0({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64},
                     {"left_only", "col1", "col2", "col3"});
  auto mem_src = MakeMemSource(relation0);

  auto join_node =
      MakeJoin({mem_src, mem_src}, "inner", relation0, relation0, {"col1"}, {"col1"}, {"_x", "_y"});

  std::vector<IRNode*> map_nodes = graph->FindNodesOfType(IRNodeType::kMap);
  EXPECT_EQ(map_nodes.size(), 1);
  auto map_node = static_cast<MapIR*>(map_nodes[0]);
  EXPECT_THAT(join_node->parents(), ElementsAre(mem_src, map_node));
}

TEST_F(OpTests, union_duplicate_parents) {
  Relation relation0({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64},
                     {"left_only", "col1", "col2", "col3"});
  auto mem_src1 = MakeMemSource(relation0);

  Relation relation1({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64, types::DataType::INT64},
                     {"right_only", "col1", "col2", "col3", "col4"});
  auto mem_src2 = MakeMemSource(relation1);
  auto union_op = MakeUnion({mem_src2, mem_src1, mem_src1, mem_src1});

  std::vector<IRNode*> maps = graph->FindNodesOfType(IRNodeType::kMap);
  ASSERT_EQ(maps.size(), 2);

  EXPECT_THAT(union_op->parents(), UnorderedElementsAre(mem_src2, mem_src1, maps[0], maps[1]));
}
constexpr char kOpenNetworkConnsUDTFSourceSpecPb[] = R"proto(
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

constexpr char kExpectedUDTFSourceOpSingleArgPb[] = R"proto(
  op_type: UDTF_SOURCE_OPERATOR
  udtf_source_op {
    name: "OpenNetworkConnections"
    arg_values {
      data_type: STRING
      string_value: "5525adaadadadadad"
    }
  }
)proto";

TEST_F(OpTests, UDTFSingleArgTest) {
  udfspb::UDTFSourceSpec udtf_spec;
  ASSERT_TRUE(
      google::protobuf::TextFormat::MergeFromString(kOpenNetworkConnsUDTFSourceSpecPb, &udtf_spec));

  Relation relation{{types::TIME64NS, types::INT64, types::STRING}, {"time_", "fd", "name"}};

  absl::flat_hash_map<std::string, ExpressionIR*> arg_map{
      {"upid", MakeString("5525adaadadadadad")}};

  auto udtf_or_s =
      graph->CreateNode<UDTFSourceIR>(ast, "OpenNetworkConnections", arg_map, udtf_spec);
  ASSERT_OK(udtf_or_s);
  UDTFSourceIR* udtf = udtf_or_s.ConsumeValueOrDie();

  planpb::Operator pb;
  EXPECT_OK(udtf->ToProto(&pb));
  EXPECT_THAT(pb, EqualsProto(kExpectedUDTFSourceOpSingleArgPb)) << pb.DebugString();

  EXPECT_THAT(*udtf->resolved_table_type(), IsTableType(relation));
}

constexpr char kDiskSpaceUDTFPb[] = R"proto(
name: "GetDiskSpace"
args {
  name: "agent"
  arg_type: STRING
  semantic_type: ST_AGENT_UID
}
args {
  name: "disk_idx"
  arg_type: INT64
  semantic_type: ST_NONE
}
executor: UDTF_SUBSET_PEM
relation {
  columns {
    column_name: "used_capacity"
    column_type: INT64
    column_semantic_type: ST_NONE
  }
  columns {
    column_name: "total_capacity"
    column_type: INT64
    column_semantic_type: ST_NONE
  }
}
)proto";

constexpr char kExpectedUDTFSourceOpMultipleArgsPb[] = R"proto(
  op_type: UDTF_SOURCE_OPERATOR
  udtf_source_op {
    name: "GetDiskSpace"
    arg_values {
      data_type: STRING
      string_value: "5525adaadadadadad"
    }
    arg_values {
      data_type: INT64
      int64_value: 321
    }
  }
)proto";

TEST_F(OpTests, UDTFMultipleOutOfOrderArgs) {
  udfspb::UDTFSourceSpec udtf_spec;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kDiskSpaceUDTFPb, &udtf_spec));

  Relation relation{{types::INT64, types::INT64}, {"used_capacity", "total_capacity"}};

  absl::flat_hash_map<std::string, ExpressionIR*> arg_map{
      {"disk_idx", MakeInt(321)}, {"agent", MakeString("5525adaadadadadad")}};

  auto udtf_or_s = graph->CreateNode<UDTFSourceIR>(ast, "GetDiskSpace", arg_map, udtf_spec);
  ASSERT_OK(udtf_or_s);
  UDTFSourceIR* udtf = udtf_or_s.ConsumeValueOrDie();

  planpb::Operator pb;
  EXPECT_OK(udtf->ToProto(&pb));
  EXPECT_THAT(pb, EqualsProto(kExpectedUDTFSourceOpMultipleArgsPb)) << pb.DebugString();

  EXPECT_THAT(*udtf->resolved_table_type(), IsTableType(relation));
}

TEST_F(OpTests, uint128_ir) {
  auto uint128_or_s = graph->CreateNode<UInt128IR>(ast, absl::MakeUint128(123, 456));
  ASSERT_OK(uint128_or_s);
  auto uint128ir = uint128_or_s.ConsumeValueOrDie();
  EXPECT_EQ(uint128ir->val(), absl::MakeUint128(123, 456));
}

TEST_F(OpTests, uint128_ir_init_from_str) {
  std::string uuid_str = "00000000-0000-007b-0000-0000000001c8";
  auto uint128_or_s = graph->CreateNode<UInt128IR>(ast, uuid_str);
  ASSERT_OK(uint128_or_s);
  auto uint128ir = uint128_or_s.ConsumeValueOrDie();
  EXPECT_EQ(uint128ir->val(), absl::MakeUint128(123, 456));
}

TEST_F(OpTests, uint128_ir_init_from_str_bad_format) {
  std::string uuid_str = "bad_uuid";
  auto uint128_or_s = graph->CreateNode<UInt128IR>(ast, uuid_str);
  ASSERT_NOT_OK(uint128_or_s);
  EXPECT_THAT(uint128_or_s.status(), HasCompilerError(".* is not a valid UUID"));
}

TEST_F(OpTests, grpc_sink_required_inputs) {
  auto mem_source =
      graph->CreateNode<MemorySourceIR>(ast, "source", std::vector<std::string>{}).ValueOrDie();
  auto sink = graph
                  ->CreateNode<GRPCSinkIR>(ast, mem_source, "output_table",
                                           std::vector<std::string>{"output1", "output2"})
                  .ValueOrDie();

  auto rel = table_store::schema::Relation(
      std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64}),
      std::vector<std::string>({"output1", "output2"}));
  compiler_state_->relation_map()->emplace("source", rel);

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto inputs = sink->RequiredInputColumns().ConsumeValueOrDie();
  EXPECT_EQ(1, inputs.size());
  EXPECT_THAT(inputs[0], UnorderedElementsAre("output1", "output2"));
}

using RequiredInputTest = ToProtoTest;
TEST_F(RequiredInputTest, map_required_inputs) {
  Relation rel({types::INT64, types::INT64, types::INT64}, {"test1", "test2", "test3"});
  MemorySourceIR* mem_source = MakeMemSource("table", rel);
  compiler_state_->relation_map()->emplace("table", rel);
  ColumnIR* col1 = MakeColumn("test1", /*parent_op_idx*/ 0);
  ColumnIR* col2 = MakeColumn("test2", /*parent_op_idx*/ 0);
  ColumnIR* col3 = MakeColumn("test3", /*parent_op_idx*/ 0);
  FuncIR* add_func = MakeAddFunc(col3, MakeInt(3));
  MapIR* child_map =
      MakeMap(mem_source, {{{"out11", col1}, {"out2", col2}, {"out3", add_func}}, {}});

  ASSERT_OK(ResolveOperatorType(mem_source, compiler_state_.get()));
  ASSERT_OK(ResolveOperatorType(child_map, compiler_state_.get()));

  auto inputs = child_map->RequiredInputColumns().ConsumeValueOrDie();
  EXPECT_EQ(1, inputs.size());
  EXPECT_THAT(inputs[0], UnorderedElementsAre("test1", "test2", "test3"));
}

TEST_F(RequiredInputTest, filter_required_inputs) {
  auto rel = MakeRelation();
  auto mem_src = MakeMemSource("table", rel);
  compiler_state_->relation_map()->emplace("table", rel);
  auto equals_fn = MakeEqualsFunc(MakeColumn("cpu0", 0), MakeColumn("cpu2", 0));
  auto filter = MakeFilter(mem_src, equals_fn);

  ASSERT_OK(ResolveOperatorType(mem_src, compiler_state_.get()));
  ASSERT_OK(ResolveOperatorType(filter, compiler_state_.get()));

  auto inputs = filter->RequiredInputColumns().ConsumeValueOrDie();
  EXPECT_EQ(1, inputs.size());
  EXPECT_THAT(inputs[0], UnorderedElementsAre("count", "cpu0", "cpu1", "cpu2"));
}

TEST_F(RequiredInputTest, blocking_agg_required_inputs) {
  auto rel = MakeRelation();
  auto mem_src = MakeMemSource("table", rel);
  compiler_state_->relation_map()->emplace("table", rel);
  auto mean_func = MakeMeanFunc(MakeColumn("cpu0", 0));
  auto agg = MakeBlockingAgg(mem_src, {MakeColumn("cpu1", 0)}, {{"mean", mean_func}});

  ASSERT_OK(ResolveOperatorType(mem_src, compiler_state_.get()));
  ASSERT_OK(ResolveOperatorType(agg, compiler_state_.get()));

  auto inputs = agg->RequiredInputColumns().ConsumeValueOrDie();
  EXPECT_EQ(1, inputs.size());
  EXPECT_THAT(inputs[0], UnorderedElementsAre("cpu0", "cpu1"));
}

TEST_F(RequiredInputTest, join_required_inputs) {
  Relation relation0({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64},
                     {"left_only", "col1", "col2", "col3"});
  auto mem_src1 = MakeMemSource(relation0);

  Relation relation1({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64, types::DataType::INT64, types::DataType::INT64},
                     {"right_only", "col1", "col2", "col3", "col4", "col5"});
  auto mem_src2 = MakeMemSource(relation1);

  std::vector<std::string> col_names{"left_only", "col4", "col1", "right_only"};

  // inner join
  auto join =
      MakeJoin({mem_src1, mem_src2}, "inner", relation0, relation1,
               std::vector<std::string>{"col1", "col3"}, std::vector<std::string>{"col2", "col4"});
  std::vector<ColumnIR*> cols{MakeColumn("left_only", 0), MakeColumn("col4", 1),
                              MakeColumn("col1", 0), MakeColumn("right_only", 1)};
  EXPECT_OK(join->SetOutputColumns(col_names, cols));

  auto inputs = join->RequiredInputColumns().ConsumeValueOrDie();
  EXPECT_EQ(2, inputs.size());
  EXPECT_THAT(inputs[0], UnorderedElementsAre("left_only", "col1", "col3"));
  EXPECT_THAT(inputs[1], UnorderedElementsAre("right_only", "col2", "col4"));

  // right join
  std::vector<ColumnIR*> left_on{MakeColumn("col1", 1), MakeColumn("col3", 1)};
  std::vector<ColumnIR*> right_on{MakeColumn("col2", 0), MakeColumn("col4", 0)};

  auto right_join =
      graph
          ->CreateNode<JoinIR>(ast, std::vector<OperatorIR*>{mem_src1, mem_src2}, "right", left_on,
                               right_on, std::vector<std::string>{"_x", "_y"})
          .ConsumeValueOrDie();
  std::vector<ColumnIR*> right_cols{MakeColumn("left_only", 1), MakeColumn("col4", 0),
                                    MakeColumn("col1", 1), MakeColumn("right_only", 0)};
  EXPECT_OK(right_join->SetOutputColumns(col_names, right_cols));

  auto right_inputs = right_join->RequiredInputColumns().ConsumeValueOrDie();
  EXPECT_EQ(2, right_inputs.size());
  EXPECT_THAT(right_inputs[0], UnorderedElementsAre("right_only", "col2", "col4"));
  EXPECT_THAT(right_inputs[1], UnorderedElementsAre("left_only", "col1", "col3"));
}

TEST_F(OpTests, prune_outputs) {
  Relation original_relation{{types::DataType::INT64, types::DataType::FLOAT64,
                              types::DataType::FLOAT64, types::DataType::FLOAT64},
                             {"count", "cpu0", "cpu1", "cpu2"}};
  Relation expected_relation{{types::DataType::FLOAT64, types::DataType::FLOAT64},
                             {"cpu0", "cpu2"}};

  auto mem_src = MakeMemSource("source", original_relation);
  compiler_state_->relation_map()->emplace("source", original_relation);

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  EXPECT_OK(mem_src->PruneOutputColumnsTo({"cpu0", "cpu2"}));
  EXPECT_THAT(mem_src->column_names(), ElementsAre("cpu0", "cpu2"));
  EXPECT_THAT(mem_src->column_index_map(), ElementsAre(1, 3));

  // Check that the top-level func updated the relation.
  EXPECT_THAT(*mem_src->resolved_table_type(), IsTableType(expected_relation));
}

TEST_F(OpTests, prune_outputs_unchanged) {
  auto mem_src = MakeMemSource("foo");
  compiler_state_->relation_map()->emplace("foo", MakeRelation());
  mem_src->SetColumnIndexMap({0, 1, 2, 3});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  EXPECT_OK(mem_src->PruneOutputColumnsTo({"count", "cpu0", "cpu1", "cpu2"}));
  EXPECT_TRUE(mem_src->select_all());
  EXPECT_THAT(mem_src->column_index_map(), ElementsAre(0, 1, 2, 3));
}

TEST_F(OpTests, prune_outputs_keep_one) {
  auto mem_src = MakeMemSource("foo", MakeRelation());
  compiler_state_->relation_map()->emplace("foo", MakeRelation());
  mem_src->SetColumnIndexMap({0, 1, 2, 3});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  EXPECT_OK(mem_src->PruneOutputColumnsTo({}));
  EXPECT_THAT(mem_src->column_names(), ElementsAre("count"));
  EXPECT_THAT(mem_src->column_index_map(), ElementsAre(0));
}

TEST_F(OpTests, map_prune_outputs) {
  auto mem_src = MakeMemSource("foo", MakeRelation());
  compiler_state_->relation_map()->emplace("foo", MakeRelation());
  auto map = MakeMap(mem_src, {{"count", MakeColumn("count", 0)},
                               {"cpu0", MakeColumn("cpu0", 0)},
                               {"cpu1", MakeColumn("cpu1", 0)},
                               {"cpu2", MakeColumn("cpu2", 0)}});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  EXPECT_OK(map->PruneOutputColumnsTo({"cpu1"}));
  auto exprs = map->col_exprs();
  EXPECT_EQ(1, exprs.size());
  EXPECT_EQ("cpu1", exprs[0].name);
  EXPECT_MATCH(exprs[0].node, ColumnNode("cpu1", /*parent_idx*/ 0));
}

TEST_F(OpTests, filter_prune_outputs) {
  auto mem_src = MakeMemSource("foo", MakeRelation());
  compiler_state_->relation_map()->emplace("foo", MakeRelation());
  auto filter = MakeFilter(mem_src, MakeEqualsFunc(MakeColumn("cpu0", 0), MakeColumn("cpu1", 0)));

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  EXPECT_OK(filter->PruneOutputColumnsTo({"cpu1"}));
  EXPECT_THAT(filter->resolved_table_type()->ColumnNames(), ElementsAre("cpu1"));
}

TEST_F(OpTests, agg_prune_outputs) {
  auto mem_src = MakeMemSource("foo", MakeRelation());
  compiler_state_->relation_map()->emplace("foo", MakeRelation());
  auto agg = MakeBlockingAgg(mem_src, {MakeColumn("count", 0)},
                             {{"cpu0", MakeMeanFunc(MakeColumn("cpu0", 0))},
                              {"cpu1", MakeMeanFunc(MakeColumn("cpu1", 0))},
                              {"cpu2", MakeMeanFunc(MakeColumn("cpu2", 0))}});
  auto old_groups = agg->groups();

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  EXPECT_OK(agg->PruneOutputColumnsTo({"cpu0"}));

  // Groups shouldn't have changed
  EXPECT_EQ(old_groups, agg->groups());
  auto agg_exprs = agg->aggregate_expressions();
  EXPECT_EQ(1, agg_exprs.size());
  EXPECT_EQ("cpu0", agg_exprs[0].name);
  EXPECT_MATCH(agg_exprs[0].node, Func());
}

TEST_F(OpTests, union_prune_outputs) {
  Relation relation1 = MakeRelation();
  Relation relation2{{types::DataType::FLOAT64, types::DataType::FLOAT64, types::DataType::INT64,
                      types::DataType::FLOAT64},
                     {"cpu1", "cpu2", "count", "cpu0"}};
  auto mem_src1 = MakeMemSource("source1", relation1);
  compiler_state_->relation_map()->emplace("source1", relation1);
  auto mem_src2 = MakeMemSource("source2", relation2);
  compiler_state_->relation_map()->emplace("source2", relation2);
  auto union_op = MakeUnion({mem_src1, mem_src2});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  EXPECT_OK(union_op->PruneOutputColumnsTo({"cpu2", "count"}));
  EXPECT_EQ(2, union_op->column_mappings().size());

  for (const auto& column_mapping : union_op->column_mappings()) {
    std::vector<std::string> colnames{column_mapping[0]->col_name(), column_mapping[1]->col_name()};
    EXPECT_THAT(colnames, ElementsAre("count", "cpu2"));
  }
}

TEST_F(OpTests, join_prune_outputs) {
  Relation relation0({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64},
                     {"left_only", "col1", "col2", "col3"});
  auto mem_src1 = MakeMemSource("source0", relation0);
  compiler_state_->relation_map()->emplace("source0", relation0);

  Relation relation1({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64, types::DataType::INT64},
                     {"right_only", "col1", "col2", "col3", "col4"});
  auto mem_src2 = MakeMemSource("source1", relation1);
  compiler_state_->relation_map()->emplace("source1", relation1);

  auto join_op = MakeJoin(
      {mem_src1, mem_src2}, "inner", relation0, relation1, std::vector<std::string>{"col1", "col3"},
      std::vector<std::string>{"col2", "col4"}, std::vector<std::string>{"_left", ""});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  EXPECT_THAT(join_op->column_names(),
              ElementsAre("left_only", "col1_left", "col2_left", "col3_left", "right_only", "col1",
                          "col2", "col3", "col4"));

  EXPECT_OK(join_op->PruneOutputColumnsTo({"left_only", "right_only", "col1", "col2_left"}));

  EXPECT_THAT(join_op->column_names(), ElementsAre("left_only", "col2_left", "right_only", "col1"));

  std::vector<std::string> expected_inputs{"left_only", "col2", "right_only", "col1"};
  for (const auto& [i, expect_colname] : Enumerate(expected_inputs)) {
    EXPECT_EQ(expect_colname, join_op->output_columns()[i]->col_name());
  }
}

TEST(NodeMatches, CompareAllDataTypes) {
  auto ast = MakeTestAstPtr();
  auto ir = std::make_shared<IR>();
  // Make a Node for each type.
  ASSERT_OK_AND_ASSIGN(auto bool_ir, ir->CreateNode<BoolIR>(ast, true));
  ASSERT_OK_AND_ASSIGN(auto int_ir, ir->CreateNode<IntIR>(ast, 1));
  ASSERT_OK_AND_ASSIGN(auto float_ir, ir->CreateNode<FloatIR>(ast, 1.0));
  ASSERT_OK_AND_ASSIGN(auto string_ir, ir->CreateNode<StringIR>(ast, "foo"));
  ASSERT_OK_AND_ASSIGN(auto time_ir, ir->CreateNode<TimeIR>(ast, 1));
  ASSERT_OK_AND_ASSIGN(auto uint128_ir,
                       ir->CreateNode<UInt128IR>(ast, "01234567-89ab-cdef-0123-456789abcdef"));

  std::vector<DataIR*> nodes{bool_ir, int_ir, float_ir, string_ir, time_ir, uint128_ir};
  // A vector of functions for each CompareNodes function.
  std::vector<std::function<bool(IRNode*)>> compare_funcs{
      BoolIR::NodeMatches,   IntIR::NodeMatches,  FloatIR::NodeMatches,
      StringIR::NodeMatches, TimeIR::NodeMatches, UInt128IR::NodeMatches};
  ASSERT_EQ(nodes.size(), compare_funcs.size());
  for (const auto& [i, node] : Enumerate(nodes)) {
    for (const auto& [j, compare_func] : Enumerate(compare_funcs)) {
      if (i == j) {
        EXPECT_TRUE(compare_func(node));
      } else {
        EXPECT_FALSE(compare_func(node));
      }
    }
  }
}

TEST(ZeroValueForType, TestBool) {
  auto ir = std::make_shared<IR>();
  auto val = DataIR::ZeroValueForType(ir.get(), IRNodeType::kBool).ConsumeValueOrDie();
  EXPECT_EQ(IRNodeType::kBool, val->type());
  EXPECT_EQ(false, static_cast<BoolIR*>(val)->val());
}

TEST(ZeroValueForType, TestFloat) {
  auto ir = std::make_shared<IR>();
  auto val = DataIR::ZeroValueForType(ir.get(), IRNodeType::kFloat).ConsumeValueOrDie();
  EXPECT_EQ(IRNodeType::kFloat, val->type());
  EXPECT_EQ(0, static_cast<FloatIR*>(val)->val());
}

TEST(ZeroValueForType, TestInt) {
  auto ir = std::make_shared<IR>();
  auto val = DataIR::ZeroValueForType(ir.get(), IRNodeType::kInt).ConsumeValueOrDie();
  EXPECT_EQ(IRNodeType::kInt, val->type());
  EXPECT_EQ(0, static_cast<IntIR*>(val)->val());
}

TEST(ZeroValueForType, TestString) {
  auto ir = std::make_shared<IR>();
  auto val = DataIR::ZeroValueForType(ir.get(), IRNodeType::kString).ConsumeValueOrDie();
  EXPECT_EQ(IRNodeType::kString, val->type());
  EXPECT_EQ("", static_cast<StringIR*>(val)->str());
}

TEST(ZeroValueForType, TestTime) {
  auto ir = std::make_shared<IR>();
  auto val = DataIR::ZeroValueForType(ir.get(), IRNodeType::kTime).ConsumeValueOrDie();
  EXPECT_EQ(IRNodeType::kTime, val->type());
  EXPECT_EQ(0, static_cast<TimeIR*>(val)->val());
}

TEST(ZeroValueForType, TestUint128) {
  auto ir = std::make_shared<IR>();
  auto val = DataIR::ZeroValueForType(ir.get(), IRNodeType::kUInt128).ConsumeValueOrDie();
  EXPECT_EQ(IRNodeType::kUInt128, val->type());
  EXPECT_EQ(0, static_cast<UInt128IR*>(val)->val());
}

using SplitFuncTest = ToProtoTest;
TEST_F(SplitFuncTest, SplitInitArgs) {
  auto constant = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto col = graph->CreateNode<ColumnIR>(ast, "col4", /*parent_op_idx*/ 0).ValueOrDie();
  EXPECT_OK(col->SetResolvedType(ValueType::Create(types::INT64, types::ST_NONE)));
  auto func = graph
                  ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::add, "+", "add"},
                                       std::vector<ExpressionIR*>({constant, col}))
                  .ValueOrDie();

  EXPECT_EQ(2, func->all_args().size());

  ASSERT_OK(AddUDFToRegistry("add", types::INT64, {types::INT64}, {types::INT64}));
  ASSERT_OK(ResolveExpressionType(func, compiler_state_.get(), {}));

  EXPECT_EQ(1, func->init_args().size());
  EXPECT_EQ(constant, func->init_args()[0]);
  EXPECT_EQ(1, func->args().size());
  EXPECT_EQ(col, func->args()[0]);
}

TEST_F(SplitFuncTest, UpdateArgWorksWithSplit) {
  auto constant = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto col = graph->CreateNode<ColumnIR>(ast, "col4", /*parent_op_idx*/ 0).ValueOrDie();
  EXPECT_OK(col->SetResolvedType(ValueType::Create(types::INT64, types::ST_NONE)));
  auto func = graph
                  ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::add, "+", "add"},
                                       std::vector<ExpressionIR*>({constant, col}))
                  .ValueOrDie();

  ASSERT_OK(AddUDFToRegistry("add", types::INT64, {types::INT64}, {types::INT64}));
  ASSERT_OK(ResolveExpressionType(func, compiler_state_.get(), {}));

  ASSERT_OK(func->UpdateArg(constant, graph->CreateNode<IntIR>(ast, 20).ConsumeValueOrDie()));
  ASSERT_OK(func->UpdateArg(col, graph->CreateNode<IntIR>(ast, 400).ConsumeValueOrDie()));
  EXPECT_MATCH(func->init_args()[0], Int(20));
  EXPECT_MATCH(func->args()[0], Int(400));
}

}  // namespace planner

}  // namespace carnot
}  // namespace px
