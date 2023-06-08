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

#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/analyzer/resolve_types_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/splitter/splitter.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/planner/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {
using compiler::ResolveTypesRule;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

class SplitterTest : public ASTVisitorTest {
 protected:
  void SetUp() override {
    ASTVisitorTest::SetUp();
    cpu_relation = table_store::schema::Relation(
        std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64,
                                      types::DataType::FLOAT64, types::DataType::FLOAT64}),
        std::vector<std::string>({"count", "cpu0", "cpu1", "cpu2"}));
    compiler_state_->relation_map()->erase("cpu");
    compiler_state_->relation_map()->emplace("cpu", cpu_relation);

    PX_CHECK_OK(compiler_state_->registry_info()->Init(testutils::UDFInfoWithTestUDTF()));
  }
  void HasGRPCSinkChild(int64_t id, IR* test_graph, const std::string& err_string) {
    IRNode* maybe_op_node = test_graph->Get(id);
    ASSERT_TRUE(Match(maybe_op_node, Operator())) << err_string;
    OperatorIR* op = static_cast<OperatorIR*>(maybe_op_node);
    ASSERT_EQ(op->Children().size(), 1) << err_string;
    OperatorIR* map_child = op->Children()[0];
    EXPECT_EQ(map_child->type(), IRNodeType::kGRPCSink) << err_string;
  }
  void HasGRPCSourceGroupParent(int64_t id, IR* test_graph, const std::string& err_string) {
    IRNode* maybe_op_node = test_graph->Get(id);
    ASSERT_TRUE(Match(maybe_op_node, Operator())) << err_string;
    OperatorIR* op = static_cast<OperatorIR*>(maybe_op_node);
    ASSERT_EQ(op->parents().size(), 1);
    OperatorIR* sink_parent = op->parents()[0];
    EXPECT_EQ(sink_parent->type(), IRNodeType::kGRPCSourceGroup);
  }

  template <typename TIR>
  bool HasEquivalentInNewPlan(IR* new_graph, TIR* old_node) {
    return new_graph->HasNode(old_node->id());
  }

  template <typename TIR>
  TIR* GetEquivalentInNewPlan(IR* new_graph, TIR* old_node) {
    DCHECK(new_graph->HasNode(old_node->id())) << old_node->DebugString() << " not found";
    IRNode* new_node = new_graph->Get(old_node->id());
    DCHECK_EQ(new_node->type(), old_node->type());
    return static_cast<TIR*>(new_node);
  }

  int64_t time_now = 1552607213931245000;
  table_store::schema::Relation cpu_relation;
};

TEST_F(SplitterTest, blocking_agg_test) {
  auto mem_src = MakeMemSource("cpu", cpu_relation);
  auto mean_func = MakeMeanFuncWithFloatType(MakeColumn("count", 0, types::DataType::INT64));
  auto agg = MakeBlockingAgg(mem_src, {MakeColumn("count", 0, types::DataType::INT64)},
                             {{"mean", mean_func}});
  auto sink = MakeMemSink(agg, "out");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();

  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();

  // Verify the resultant graph.
  MemorySourceIR* new_mem_src = GetEquivalentInNewPlan(before_blocking, mem_src);
  ASSERT_EQ(new_mem_src->Children().size(), 1UL);
  OperatorIR* mem_src_child = new_mem_src->Children()[0];
  ASSERT_TRUE(Match(mem_src_child, GRPCSink()))
      << "Expected GRPCSink, got " << mem_src_child->type_string();
  GRPCSinkIR* grpc_sink = static_cast<GRPCSinkIR*>(mem_src_child);

  OperatorIR* agg_parent = GetEquivalentInNewPlan(after_blocking, agg)->parents()[0];
  ASSERT_TRUE(Match(agg_parent, GRPCSourceGroup()))
      << "Expected GRPCSourceGroup, got " << agg_parent->type_string();
  GRPCSourceGroupIR* grpc_source_group = static_cast<GRPCSourceGroupIR*>(agg_parent);

  EXPECT_EQ(grpc_sink->destination_id(), grpc_source_group->source_id());

  OperatorIR* sink_parent = GetEquivalentInNewPlan(after_blocking, sink)->parents()[0];
  EXPECT_MATCH(sink_parent, BlockingAgg());
}

TEST_F(SplitterTest, partial_agg_test) {
  auto mem_src = MakeMemSource("cpu", cpu_relation);
  auto count_col = MakeColumn("count", 0, types::DataType::INT64);
  EXPECT_OK(count_col->SetResolvedType(ValueType::Create(types::INT64, types::ST_NONE)));
  auto mean_func = MakeMeanFuncWithFloatType(MakeColumn("count", 0, types::DataType::INT64));
  ASSERT_OK(AddUDAToRegistry("mean", types::FLOAT64, {types::INT64}, /*supports_partial*/ true));
  auto agg = MakeBlockingAgg(mem_src, {count_col}, {{"mean", mean_func}});

  table_store::schema::Relation relation({types::INT64, types::FLOAT64}, {"count", "mean"});
  MakeMemSink(agg, "out");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ true);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();

  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();

  // Verify the resultant graph.
  MemorySourceIR* new_mem_src = GetEquivalentInNewPlan(before_blocking, mem_src);
  ASSERT_EQ(new_mem_src->Children().size(), 1UL);
  EXPECT_MATCH(new_mem_src->Children()[0], PartialAgg());

  auto partial_agg = static_cast<BlockingAggIR*>(new_mem_src->Children()[0]);
  ASSERT_EQ(partial_agg->Children().size(), 1UL);
  ASSERT_MATCH(partial_agg->Children()[0], GRPCSink());

  GRPCSinkIR* grpc_sink = static_cast<GRPCSinkIR*>(partial_agg->Children()[0]);

  auto grpc_source_groups = after_blocking->FindNodesThatMatch(GRPCSourceGroup());
  ASSERT_EQ(grpc_source_groups.size(), 1);
  GRPCSourceGroupIR* grpc_source = static_cast<GRPCSourceGroupIR*>(grpc_source_groups[0]);
  ASSERT_EQ(grpc_source->Children().size(), 1);
  ASSERT_MATCH(grpc_source->Children()[0], FinalizeAgg());

  BlockingAggIR* finalize_agg = static_cast<BlockingAggIR*>(grpc_source->Children()[0]);

  EXPECT_EQ(grpc_sink->destination_id(), grpc_source->source_id());

  // Confirm that the relations have serialized in their relation.
  EXPECT_THAT(*grpc_sink->resolved_table_type(),
              IsTableType(Relation({types::INT64, types::STRING}, {"count", "serialized_mean"})));
  EXPECT_THAT(*grpc_source->resolved_table_type(),
              IsTableType(Relation({types::INT64, types::STRING}, {"count", "serialized_mean"})));

  // Verify that the aggregate connects back into the original group.
  ASSERT_EQ(finalize_agg->Children().size(), 1);
  EXPECT_MATCH(finalize_agg->Children()[0], MemorySink());

  ASSERT_EQ(partial_agg->aggregate_expressions().size(),
            finalize_agg->aggregate_expressions().size());
  for (int64_t i = 0; i < static_cast<int64_t>(partial_agg->aggregate_expressions().size()); ++i) {
    auto prep_expr = partial_agg->aggregate_expressions()[i];
    auto finalize_expr = finalize_agg->aggregate_expressions()[i];
    EXPECT_EQ(prep_expr.name, finalize_expr.name);
    EXPECT_TRUE(prep_expr.node->Equals(finalize_expr.node))
        << absl::Substitute("prep expr $0 finalize expr $1", prep_expr.node->DebugString(),
                            finalize_expr.node->DebugString());
  }
}

TEST_F(SplitterTest, limit_test) {
  auto mem_src = MakeMemSource("cpu", cpu_relation);
  auto limit = MakeLimit(mem_src, 10);
  auto sink = MakeMemSink(limit, "out");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();

  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();

  // Verify the resultant graph.
  MemorySourceIR* new_mem_src = GetEquivalentInNewPlan(before_blocking, mem_src);
  ASSERT_EQ(new_mem_src->Children().size(), 1UL) << new_mem_src->ChildrenDebugString();
  OperatorIR* mem_src_child = new_mem_src->Children()[0];

  ASSERT_TRUE(Match(mem_src_child, Limit()))
      << "Expected Limit, got " << mem_src_child->type_string();
  LimitIR* new_limit = static_cast<LimitIR*>(mem_src_child);
  EXPECT_EQ(new_limit->limit_value(), limit->limit_value());
  ASSERT_EQ(new_limit->Children().size(), 1UL);

  OperatorIR* before_blocking_limit_child = new_limit->Children()[0];
  ASSERT_TRUE(Match(before_blocking_limit_child, GRPCSink()))
      << "Expected GRPCSink, got " << before_blocking_limit_child->type_string();
  GRPCSinkIR* grpc_sink = static_cast<GRPCSinkIR*>(before_blocking_limit_child);

  OperatorIR* sink_parent = GetEquivalentInNewPlan(after_blocking, sink)->parents()[0];
  ASSERT_TRUE(Match(sink_parent, Limit())) << "Expected Limit, got " << sink_parent->type_string();
  LimitIR* after_blocking_limit = static_cast<LimitIR*>(sink_parent);
  EXPECT_EQ(after_blocking_limit->limit_value(), limit->limit_value());

  OperatorIR* limit_parent = after_blocking_limit->parents()[0];
  ASSERT_TRUE(Match(limit_parent, GRPCSourceGroup()))
      << "Expected GRPCSourceGroup, got " << limit_parent->type_string();
  GRPCSourceGroupIR* grpc_source_group = static_cast<GRPCSourceGroupIR*>(limit_parent);

  EXPECT_EQ(grpc_sink->destination_id(), grpc_source_group->source_id());
}

TEST_F(SplitterTest, limit_test_pem_only) {
  auto mem_src = MakeMemSource("cpu", cpu_relation);
  auto limit = MakeLimit(mem_src, 10, /* pem_only */ true);
  auto sink = MakeMemSink(limit, "out");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();

  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();

  // Verify the resultant graph.
  MemorySourceIR* new_mem_src = GetEquivalentInNewPlan(before_blocking, mem_src);
  ASSERT_EQ(new_mem_src->Children().size(), 1UL) << new_mem_src->ChildrenDebugString();
  OperatorIR* mem_src_child = new_mem_src->Children()[0];

  ASSERT_TRUE(Match(mem_src_child, Limit()))
      << "Expected Limit, got " << mem_src_child->type_string();
  LimitIR* new_limit = static_cast<LimitIR*>(mem_src_child);
  EXPECT_EQ(new_limit->limit_value(), limit->limit_value());
  ASSERT_EQ(new_limit->Children().size(), 1UL);

  OperatorIR* before_blocking_limit_child = new_limit->Children()[0];
  ASSERT_TRUE(Match(before_blocking_limit_child, GRPCSink()))
      << "Expected GRPCSink, got " << before_blocking_limit_child->type_string();
  GRPCSinkIR* grpc_sink = static_cast<GRPCSinkIR*>(before_blocking_limit_child);

  OperatorIR* sink_parent = GetEquivalentInNewPlan(after_blocking, sink)->parents()[0];
  ASSERT_TRUE(Match(sink_parent, GRPCSourceGroup()))
      << "Expected GRPCSourceGroup, got " << sink_parent->type_string();
  GRPCSourceGroupIR* grpc_source_group = static_cast<GRPCSourceGroupIR*>(sink_parent);

  EXPECT_EQ(grpc_sink->destination_id(), grpc_source_group->source_id());
}

TEST_F(SplitterTest, sink_only_test) {
  auto mem_src = MakeMemSource("cpu", cpu_relation);
  auto map1 = MakeMap(mem_src, {{"cpu0", MakeColumn("cpu0", 0)}, {"cpu1", MakeColumn("cpu1", 0)}});
  auto map2 = MakeMap(map1, {{"cpu0", MakeColumn("cpu0", 0)}, {"cpu1", MakeColumn("cpu1", 0)}});
  auto sink = MakeMemSink(map2, "out");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();

  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();

  std::vector<OperatorIR*> op_children = GetEquivalentInNewPlan(before_blocking, map2)->Children();
  ASSERT_EQ(op_children.size(), 1UL);
  OperatorIR* op_child = op_children[0];
  ASSERT_TRUE(Match(op_child, GRPCSink())) << "Expected GRPCSink, got " << op_child->type_string();
  GRPCSinkIR* grpc_sink = static_cast<GRPCSinkIR*>(op_child);

  OperatorIR* sink_parent = GetEquivalentInNewPlan(after_blocking, sink)->parents()[0];
  ASSERT_TRUE(Match(sink_parent, GRPCSourceGroup()))
      << "Expected GRPCSourceGroup, got " << sink_parent->type_string();
  GRPCSourceGroupIR* grpc_source_group = static_cast<GRPCSourceGroupIR*>(sink_parent);

  EXPECT_EQ(grpc_sink->destination_id(), grpc_source_group->source_id());
}

// Test to see whether splitting works when sandwiched between two separate ops.
TEST_F(SplitterTest, sandwich_test) {
  auto mem_src = MakeMemSource("cpu", cpu_relation);
  auto map = MakeMap(mem_src, {{"count", MakeColumn("count", 0, types::DataType::INT64)}});
  auto mean_func = MakeMeanFuncWithFloatType(MakeColumn("count", 0, types::DataType::INT64));
  auto agg =
      MakeBlockingAgg(map, {MakeColumn("count", 0, types::DataType::INT64)}, {{"mean", mean_func}});
  auto map2 = MakeMap(agg, {{"count", MakeColumn("count", 0, types::DataType::INT64)}});
  MakeMemSink(map2, "out");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();

  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();

  // Verify the resultant graph.
  MapIR* new_map = GetEquivalentInNewPlan(before_blocking, map);
  ASSERT_EQ(new_map->Children().size(), 1UL);
  OperatorIR* map_src_child = new_map->Children()[0];
  ASSERT_TRUE(Match(map_src_child, GRPCSink()))
      << "Expected GRPCSink, got " << map_src_child->type_string();
  GRPCSinkIR* grpc_sink = static_cast<GRPCSinkIR*>(map_src_child);

  OperatorIR* agg_parent = GetEquivalentInNewPlan(after_blocking, agg)->parents()[0];
  ASSERT_TRUE(Match(agg_parent, GRPCSourceGroup()))
      << "Expected GRPCSourceGroup, got " << agg_parent->type_string();
  GRPCSourceGroupIR* grpc_source_group = static_cast<GRPCSourceGroupIR*>(agg_parent);

  EXPECT_EQ(grpc_sink->destination_id(), grpc_source_group->source_id());
}

TEST_F(SplitterTest, first_blocking_node_test) {
  auto mem_src = MakeMemSource("cpu", cpu_relation);
  auto mean_func1 = MakeMeanFuncWithFloatType(MakeColumn("cpu0", 0, types::DataType::INT64));
  auto agg = MakeBlockingAgg(mem_src, {MakeColumn("count", 0, types::DataType::INT64)},
                             {{"mean", mean_func1}});
  auto mean_func2 = MakeMeanFuncWithFloatType(MakeColumn("mean", 0, types::DataType::FLOAT64));
  auto agg2 = MakeBlockingAgg(agg, {MakeColumn("count", 0, types::DataType::INT64)},
                              {{"mean2", mean_func2}});
  MakeMemSink(agg2, "out");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();

  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();

  // Verify the resultant graph.
  MemorySourceIR* new_mem_src = GetEquivalentInNewPlan(before_blocking, mem_src);
  ASSERT_EQ(new_mem_src->Children().size(), 1UL);
  OperatorIR* mem_src_child = new_mem_src->Children()[0];
  ASSERT_TRUE(Match(mem_src_child, GRPCSink()))
      << "Expected GRPCSink, got " << mem_src_child->type_string();
  GRPCSinkIR* grpc_sink = static_cast<GRPCSinkIR*>(mem_src_child);

  OperatorIR* agg_parent = GetEquivalentInNewPlan(after_blocking, agg)->parents()[0];
  ASSERT_TRUE(Match(agg_parent, GRPCSourceGroup()))
      << "Expected GRPCSourceGroup, got " << agg_parent->type_string();
  GRPCSourceGroupIR* grpc_source_group = static_cast<GRPCSourceGroupIR*>(agg_parent);

  EXPECT_EQ(grpc_sink->destination_id(), grpc_source_group->source_id());

  OperatorIR* agg2_parent = agg2->parents()[0];
  ASSERT_TRUE(Match(agg2_parent, BlockingAgg()))
      << "Expected BlockingAgg, got " << agg2_parent->type_string();
}

// Test feeding into unions.
TEST_F(SplitterTest, union_operator) {
  auto mem_src1 = MakeMemSource("cpu", cpu_relation);
  auto mem_src2 = MakeMemSource("cpu", cpu_relation);
  auto union_op = MakeUnion({mem_src1, mem_src2});
  MakeMemSink(union_op, "out");

  for (const auto union_parent : union_op->parents()) {
    EXPECT_MATCH(union_parent, MemorySource());
  }

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();

  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();

  std::vector<int64_t> source_group_ids;
  for (auto union_parent : GetEquivalentInNewPlan(after_blocking, union_op)->parents()) {
    ASSERT_TRUE(Match(union_parent, GRPCSourceGroup()))
        << absl::Substitute("Expected node $0 to be GRPCSourceGroup.", union_parent->DebugString());
    source_group_ids.push_back(static_cast<GRPCSourceGroupIR*>(union_parent)->source_id());
  }

  std::vector<int64_t> sink_ids;
  auto children1 = GetEquivalentInNewPlan(before_blocking, mem_src1)->Children();
  ASSERT_EQ(children1.size(), 1);
  ASSERT_TRUE(Match(children1[0], GRPCSink()))
      << absl::Substitute("Expected node $0 to be GRPCSink.", children1[0]->DebugString());

  sink_ids.push_back(static_cast<GRPCSinkIR*>(children1[0])->destination_id());

  auto children2 = GetEquivalentInNewPlan(before_blocking, mem_src2)->Children();
  ASSERT_EQ(children2.size(), 1);
  EXPECT_TRUE(Match(children2[0], GRPCSink()))
      << absl::Substitute("Expected node $0 to be GRPCSink.", children2[0]->DebugString());
  sink_ids.push_back(static_cast<GRPCSinkIR*>(children2[0])->destination_id());

  EXPECT_THAT(source_group_ids, UnorderedElementsAreArray(sink_ids));
}

/** Tests that the following graph
 *    T1
 *   /  \
 * Agg1   Agg2
 *
 * becomes:
 *    T1
 *     |
 * GRPCSink(1)
 *
 * GRPCSource(1)
 *   /  \
 * Agg1   Agg2
 */
TEST_F(SplitterTest, two_blocking_children) {
  auto mem_src = MakeMemSource("cpu", cpu_relation);
  auto mean_func1 = MakeMeanFuncWithFloatType(MakeColumn("cpu0", 0, types::DataType::INT64));
  auto blocking_agg1 = MakeBlockingAgg(mem_src, {MakeColumn("count", 0, types::DataType::INT64)},
                                       {{"cpu0_mean", mean_func1}});
  MakeMemSink(blocking_agg1, "out1");
  auto mean_func2 = MakeMeanFuncWithFloatType(MakeColumn("cpu1", 0, types::DataType::INT64));
  auto blocking_agg2 = MakeBlockingAgg(mem_src, {MakeColumn("count", 0, types::DataType::INT64)},
                                       {{"cpu1_mean", mean_func2}});
  MakeMemSink(blocking_agg2, "out2");

  EXPECT_EQ(mem_src->Children().size(), 2);

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();

  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();

  // Verify the resultant graph.
  BlockingAggIR* new_blocking_agg1 = GetEquivalentInNewPlan(after_blocking, blocking_agg1);
  ASSERT_EQ(new_blocking_agg1->parents().size(), 1);
  EXPECT_MATCH(new_blocking_agg1->parents()[0], GRPCSourceGroup());
  auto grpc_source1 = static_cast<GRPCSourceGroupIR*>(new_blocking_agg1->parents()[0]);

  BlockingAggIR* new_blocking_agg2 = GetEquivalentInNewPlan(after_blocking, blocking_agg2);
  ASSERT_EQ(new_blocking_agg2->parents().size(), 1);
  EXPECT_MATCH(new_blocking_agg2->parents()[0], GRPCSourceGroup());
  auto grpc_source2 = static_cast<GRPCSourceGroupIR*>(new_blocking_agg2->parents()[0]);

  EXPECT_EQ(grpc_source1->source_id(), grpc_source2->source_id());
  EXPECT_EQ(grpc_source1, grpc_source2);
  auto source_children = GetEquivalentInNewPlan(before_blocking, mem_src)->Children();
  ASSERT_EQ(source_children.size(), 1);
  ASSERT_MATCH(source_children[0], GRPCSink());

  auto grpc_sink1 = static_cast<GRPCSinkIR*>(source_children[0]);

  EXPECT_EQ(grpc_source1->source_id(), grpc_sink1->destination_id());
}

/** Tests the following graph:
 *    T1
 *   /  \
 * Agg   \
 *   \   /
 *    Join
 *
 * becomes
 *    T1
 *     |
 * GRPCSink(1)
 *
 * GRPCSource(1)
 *     /  \
 *   Agg   \
 *     \   /
 *      Join
 */
TEST_F(SplitterTest, agg_join_children) {
  auto mem_src = MakeMemSource("cpu", cpu_relation);
  auto mean_func = MakeMeanFuncWithFloatType(MakeColumn("cpu0", 0, types::DataType::INT64));
  auto blocking_agg = MakeBlockingAgg(mem_src, {MakeColumn("count", 0, types::DataType::INT64)},
                                      {{"cpu0_mean", mean_func}});
  auto join = MakeJoin({mem_src, blocking_agg}, "inner", MakeRelation(),
                       Relation({types::INT64, types::FLOAT64}, {"count", "cpu0_mean"}), {"count"},
                       {"count"}, {"", "_right"});
  MakeMemSink(join, "out");

  EXPECT_EQ(mem_src->Children().size(), 2);

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();

  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();

  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();

  auto new_blocking_agg = GetEquivalentInNewPlan(after_blocking, blocking_agg);
  ASSERT_EQ(new_blocking_agg->parents().size(), 1);
  ASSERT_MATCH(new_blocking_agg->parents()[0], GRPCSourceGroup());
  auto blocking_agg_parent = static_cast<GRPCSourceGroupIR*>(new_blocking_agg->parents()[0]);

  auto new_join = GetEquivalentInNewPlan(after_blocking, join);
  ASSERT_EQ(new_join->parents().size(), 2);
  // Parent 1 should be the GRPCSourceGroup
  ASSERT_MATCH(new_join->parents()[0], GRPCSourceGroup());
  auto join_parent = static_cast<GRPCSourceGroupIR*>(new_join->parents()[0]);

  EXPECT_EQ(join_parent->source_id(), blocking_agg_parent->source_id());
  EXPECT_EQ(join_parent, blocking_agg_parent);
  EXPECT_EQ(join_parent->Children().size(), 2);

  auto source_children = GetEquivalentInNewPlan(before_blocking, mem_src)->Children();
  ASSERT_EQ(source_children.size(), 1);
  ASSERT_MATCH(source_children[0], GRPCSink());

  auto grpc_sink = static_cast<GRPCSinkIR*>(source_children[0]);

  EXPECT_EQ(join_parent->source_id(), grpc_sink->destination_id());
}

TEST_F(SplitterTest, simple_split_test) {
  auto mem_src = MakeMemSource("cpu", cpu_relation);
  auto map1 = MakeMap(mem_src, {{"cpu0", MakeColumn("cpu0", 0)}, {"cpu1", MakeColumn("cpu1", 0)}});
  auto mem_sink = MakeMemSink(map1, "out");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();
  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();
  for (auto id : before_blocking->dag().TopologicalSort()) {
    IRNode* node = before_blocking->Get(id);
    EXPECT_FALSE(Match(node, BlockingOperator()) && !Match(node, GRPCSink()))
        << node->DebugString();
  }
  HasGRPCSinkChild(map1->id(), before_blocking, "");

  for (auto id : after_blocking->dag().TopologicalSort()) {
    EXPECT_FALSE(Match(after_blocking->Get(id), MemorySource()))
        << before_blocking->Get(id)->DebugString();
  }
  HasGRPCSourceGroupParent(mem_sink->id(), after_blocking, "");
}

TEST_F(SplitterTest, two_paths) {
  auto mem_src1 = MakeMemSource("cpu", cpu_relation);
  auto map1 = MakeMap(mem_src1, {{"cpu0", MakeColumn("cpu0", 0)}, {"cpu1", MakeColumn("cpu1", 0)}});
  auto mem_sink1 = MakeMemSink(map1, "out");

  auto mem_src2 = MakeMemSource("cpu", cpu_relation);
  auto map2 = MakeMap(mem_src2, {{"cpu0", MakeColumn("cpu0", 0)}, {"cpu1", MakeColumn("cpu1", 0)}});
  auto mem_sink2 = MakeMemSink(map2, "out");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();

  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();
  for (auto id : before_blocking->dag().TopologicalSort()) {
    IRNode* node = before_blocking->Get(id);
    EXPECT_FALSE(Match(node, BlockingOperator()) && !Match(node, GRPCSink()))
        << node->DebugString();
  }

  HasGRPCSinkChild(map1->id(), before_blocking, "Branch 1");
  HasGRPCSinkChild(map2->id(), before_blocking, "Branch 2");

  for (auto id : after_blocking->dag().TopologicalSort()) {
    EXPECT_FALSE(Match(after_blocking->Get(id), MemorySource()))
        << before_blocking->Get(id)->DebugString();
  }

  HasGRPCSourceGroupParent(mem_sink1->id(), after_blocking, "Branch1");
  HasGRPCSourceGroupParent(mem_sink2->id(), after_blocking, "Branch2");
}

constexpr char kUDTFOpenConnsPb[] = R"proto(
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
  }
  columns {
    column_name: "fd"
    column_type: INT64
  }
  columns {
    column_name: "name"
    column_type: STRING
  }
}
)proto";

TEST_F(SplitterTest, UDTFOnSubsetOfPEMs) {
  udfspb::UDTFSourceSpec udtf_spec;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kUDTFOpenConnsPb, &udtf_spec));
  Relation udtf_relation;
  ASSERT_OK(udtf_relation.FromProto(&udtf_spec.relation()));

  std::string upid_value = "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c";
  auto udtf = MakeUDTFSource(udtf_spec, {{"upid", MakeString(upid_value)}});
  auto sink = MakeMemSink(udtf, "out");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();
  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();

  ASSERT_TRUE(HasEquivalentInNewPlan(before_blocking, udtf));
  UDTFSourceIR* new_udtf = GetEquivalentInNewPlan(before_blocking, udtf);
  ASSERT_EQ(new_udtf->Children().size(), 1UL);

  OperatorIR* udtf_child = new_udtf->Children()[0];
  ASSERT_EQ(udtf_child->type(), IRNodeType::kGRPCSink);
  GRPCSinkIR* grpc_sink = static_cast<GRPCSinkIR*>(udtf_child);

  ASSERT_TRUE(HasEquivalentInNewPlan(after_blocking, sink));
  MemorySinkIR* new_sink = GetEquivalentInNewPlan(after_blocking, sink);
  EXPECT_EQ(new_sink->parents().size(), 1);

  // Parent 0 should be the GRPC Source Group that corresponds with the above grpc_sink.
  ASSERT_EQ(new_sink->parents()[0]->type(), IRNodeType::kGRPCSourceGroup);
  GRPCSourceGroupIR* grpc_source_group = static_cast<GRPCSourceGroupIR*>(new_sink->parents()[0]);

  EXPECT_EQ(grpc_sink->destination_id(), grpc_source_group->source_id());
}

/** Tests the following graph:
 *    ____T1_____
 *   /           \
 * Map           Map
 *  |             |
 * BlockingAgg  BlockingAgg
 *  |             |
 * Sink          Sink
 *
 * becomes
 *    T1
 *     |
 * GRPCSink(1)
 *
 *   GRPCSource(1)
 *   /           \
 * Map           Map
 *  |             |
 * BlockingAgg  BlockingAgg
 *  |             |
 * Sink          Sink
 */

TEST_F(SplitterTest, MultipleBranchedIdenticalDepths) {
  auto mem_src = MakeMemSource("cpu", cpu_relation);
  // Branch 1.
  auto map1 = MakeMap(mem_src, {{"cpu0", MakeColumn("cpu0", 0)}, {"cpu1", MakeColumn("cpu1", 0)}});
  EXPECT_OK(AddUDAToRegistry("mean_no_partial", types::FLOAT64, {types::INT64},
                             /*supports_partial*/ false));
  auto mean_func1 =
      MakeMeanFuncWithFloatType("mean_no_partial", MakeColumn("cpu0", 0, types::DataType::INT64));
  auto agg1 = MakeBlockingAgg(map1, {MakeColumn("count", 0, types::DataType::INT64)},
                              {{"mean", mean_func1}});
  MakeMemSink(agg1, "out1");
  // Branch 2.
  auto mean_func2 =
      MakeMeanFuncWithFloatType("mean_no_partial", MakeColumn("count", 0, types::DataType::INT64));
  auto map2 = MakeMap(mem_src, {{"cpu2", MakeColumn("cpu0", 0)}, {"cpu1", MakeColumn("cpu1", 0)}});
  auto agg2 = MakeBlockingAgg(map2, {MakeColumn("count", 0, types::DataType::INT64)},
                              {{"mean", mean_func2}});
  MakeMemSink(agg2, "out2");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ true);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();
  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();

  // Before should be Memsrc->GRPCSink plan.
  auto sources = before_blocking->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(sources.size(), 1);
  auto new_mem_src = static_cast<MemorySourceIR*>(sources[0]);
  ASSERT_EQ(new_mem_src->Children().size(), 1);
  ASSERT_MATCH(new_mem_src->Children()[0], GRPCSink());
  auto grpc_sink = static_cast<GRPCSinkIR*>(new_mem_src->Children()[0]);

  // After should be single grpc source group followed by the branch stuff.
  auto grpc_source_groups = after_blocking->FindNodesThatMatch(GRPCSourceGroup());
  ASSERT_EQ(grpc_source_groups.size(), 1);
  GRPCSourceGroupIR* grpc_source = static_cast<GRPCSourceGroupIR*>(grpc_source_groups[0]);

  EXPECT_EQ(grpc_source->source_id(), grpc_sink->destination_id());

  ASSERT_EQ(grpc_source->Children().size(), 2);
  ASSERT_EQ(grpc_source->Children()[0]->Children().size(), 1);
  ASSERT_EQ(grpc_source->Children()[1]->Children().size(), 1);
  // Check the types of all children.
  // Run at the end because this shouldn't block anything
  EXPECT_MATCH(grpc_source->Children()[0], Map());
  EXPECT_MATCH(grpc_source->Children()[1], Map());
  EXPECT_MATCH(grpc_source->Children()[0]->Children()[0], BlockingAgg());
  EXPECT_MATCH(grpc_source->Children()[1]->Children()[0], BlockingAgg());
}

/** Tests the following graph:
 *    ____T1_____
 *   /           \
 * Map         BlockingAgg
 *  |             |
 * BlockingAgg  Sink
 *  |
 * Sink
 *
 * becomes:
 *
 *       T1
 *        |
 *    GRPCSink(1)
 *
 *   GRPCSource(1)MultipleBranched
 *   /           \
 * Map         BlockingAgg
 *  |             |
 * BlockingAgg  Sink
 *  |
 * Sink
 */
TEST_F(SplitterTest, MultipleBranchedDifferentBlockingDepths) {
  auto mem_src = MakeMemSource("cpu", cpu_relation);
  auto map1 = MakeMap(mem_src, {{"cpu0", MakeColumn("cpu0", 0)}, {"cpu1", MakeColumn("cpu1", 0)}});
  EXPECT_OK(AddUDAToRegistry("mean_no_partial", types::FLOAT64, {types::INT64},
                             /*supports_partial*/ false));
  auto mean_func1 =
      MakeMeanFuncWithFloatType("mean_no_partial", MakeColumn("cpu0", 0, types::DataType::INT64));
  auto agg1 = MakeBlockingAgg(map1, {MakeColumn("count", 0, types::DataType::INT64)},
                              {{"mean", mean_func1}});
  MakeMemSink(agg1, "out1");

  auto mean_func2 =
      MakeMeanFuncWithFloatType("mean_no_partial", MakeColumn("count", 0, types::DataType::INT64));
  auto agg2 = MakeBlockingAgg(mem_src, {MakeColumn("count", 0, types::DataType::INT64)},
                              {{"mean", mean_func2}});
  MakeMemSink(agg2, "out2");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ true);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();
  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();

  auto sources = before_blocking->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(sources.size(), 1);

  // Before should be Memsrc->GRPCSink plan.
  auto new_mem_src = static_cast<MemorySourceIR*>(sources[0]);
  ASSERT_EQ(new_mem_src->Children().size(), 1);
  ASSERT_MATCH(new_mem_src->Children()[0], GRPCSink());
  auto grpc_sink = static_cast<GRPCSinkIR*>(new_mem_src->Children()[0]);

  // After should be grpc_source_group followed by the branching pattern.
  auto grpc_source_groups = after_blocking->FindNodesThatMatch(GRPCSourceGroup());
  ASSERT_EQ(grpc_source_groups.size(), 1);
  GRPCSourceGroupIR* grpc_source = static_cast<GRPCSourceGroupIR*>(grpc_source_groups[0]);

  EXPECT_EQ(grpc_source->source_id(), grpc_sink->destination_id());

  ASSERT_EQ(grpc_source->Children().size(), 2);
  ASSERT_EQ(grpc_source->Children()[0]->Children().size(), 1);
  // Check the types of all children.
  // Run at the end because this shouldn't block anything
  EXPECT_MATCH(grpc_source->Children()[0], Map());
  EXPECT_MATCH(grpc_source->Children()[0]->Children()[0], BlockingAgg());
  EXPECT_MATCH(grpc_source->Children()[1], BlockingAgg());
}

/** Tests the following graph:
 * Agg: Partial-friendly Aggregate.
 *
 *            ____T1_____
 *           /           \
 *    SparseFilter      Agg(2)
 *        /   \           |
 *      Map   Agg(1)     Sink
 *       |     |
 *     Sink    Sink
 *
 * becomes:
 *
 *            ________T1_______
 *           /                 \
 *   SparseFilter         PartialAgg(2)
 *         /                    |
 *    GRPCSink(1)           GRPCSink(2)
 * - - - - - - - -  - - - - - - - - - - - -
 *   GRPCSource(1)          GRPCSource(2)
 *   /           \              |
 * Map          Agg(1)     FinalizeAgg(2)
 *  |             |             |
 * Sink          Sink         Sink
 *
 * We don't  make Agg(1) partial because then we'll have to send the Map data and the Agg data which
 * will be more data sent over the network than just sending the mutual parent, assuming the Map
 * doesn't reduce the size of the batches over significantly.
 */
TEST_F(SplitterTest, MultipleBranchesAndSparseFilter) {
  table_store::schema::Relation relation(
      {types::UINT128, types::TIME64NS, types::INT64, types::DataType::FLOAT64},
      {"upid", "time_", "count", "cpu0"});
  auto mem_src = MakeMemSource("source", relation);
  compiler_state_->relation_map()->emplace("source", relation);
  // Makes a sparse filter.
  auto metadata_fn =
      MakeFunc("upid_to_service_name", {MakeColumn("upid", 0, types::DataType::UINT128)});
  metadata_fn->set_annotations(ExpressionIR::Annotations{MetadataType::SERVICE_NAME});
  auto eq_func = MakeEqualsFunc(metadata_fn, MakeString("pl/agent1"));
  auto filter = MakeFilter(mem_src, eq_func);
  auto map = MakeMap(filter, {{"time_", MakeColumn("time_", 0)},
                              {"count", MakeColumn("count", 0, types::DataType::INT64)}});
  MakeMemSink(map, "out1");

  auto count_func1 = MakeCountFunc(MakeColumn("count", 0, types::DataType::INT64));
  auto agg1 = MakeBlockingAgg(filter, {MakeColumn("count", 0, types::DataType::INT64)},
                              {{"countelms", count_func1}});
  MakeMemSink(agg1, "out2");

  auto count_func2 = MakeCountFunc(MakeColumn("count", 0, types::DataType::INT64));
  auto agg2 = MakeBlockingAgg(mem_src, {}, {{"countelms", count_func2}});
  MakeMemSink(agg2, "out3");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ true);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();
  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();

  auto sources = before_blocking->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(sources.size(), 1);

  // Before should be Memsrc->GRPCSink plan.
  auto new_mem_src = static_cast<MemorySourceIR*>(sources[0]);
  ASSERT_EQ(new_mem_src->Children().size(), 2);
  auto new_filter = new_mem_src->Children()[0];
  auto partial_agg2 = new_mem_src->Children()[1];
  ASSERT_MATCH(new_filter, Filter());
  ASSERT_MATCH(partial_agg2, PartialAgg());

  ASSERT_EQ(new_filter->Children().size(), 1);
  ASSERT_MATCH(new_filter->Children()[0], GRPCSink());
  auto new_filter_sink = static_cast<GRPCSinkIR*>(new_filter->Children()[0]);

  ASSERT_EQ(partial_agg2->Children().size(), 1);
  ASSERT_MATCH(partial_agg2->Children()[0], GRPCSink());
  auto partial_agg2_sink = static_cast<GRPCSinkIR*>(partial_agg2->Children()[0]);

  auto grpc_sources = after_blocking->FindNodesThatMatch(GRPCSourceGroup());
  ASSERT_EQ(grpc_sources.size(), 2);
  auto grpc_source0 = static_cast<GRPCSourceGroupIR*>(grpc_sources[0]);
  auto grpc_source1 = static_cast<GRPCSourceGroupIR*>(grpc_sources[1]);
  // Swap the sources so they match our expected connections.
  if (grpc_source0->source_id() != new_filter_sink->destination_id()) {
    auto tmp = grpc_source0;
    grpc_source0 = grpc_source1;
    grpc_source1 = tmp;
  }

  ASSERT_EQ(grpc_source0->source_id(), new_filter_sink->destination_id());
  ASSERT_EQ(grpc_source1->source_id(), partial_agg2_sink->destination_id());

  ASSERT_EQ(grpc_source0->Children().size(), 2);
  auto new_map = grpc_source0->Children()[0];
  auto new_agg1 = grpc_source0->Children()[1];
  EXPECT_MATCH(new_map, Map());
  EXPECT_MATCH(new_map->Children()[0], MemorySink());
  EXPECT_MATCH(new_agg1, BlockingAgg());
  EXPECT_MATCH(new_agg1->Children()[0], MemorySink());

  ASSERT_EQ(grpc_source1->Children().size(), 1);
  auto finalize_agg = grpc_source1->Children()[0];
  EXPECT_MATCH(finalize_agg, FinalizeAgg());

  ASSERT_EQ(finalize_agg->Children().size(), 1);
  EXPECT_MATCH(finalize_agg->Children()[0], MemorySink());
}

/** Tests the following graph:
 * Agg: Partial-friendly Aggregate.
 *
 *            ____T1_____
 *           /           \
 *    SparseFilter      Agg(2)
 *        /   \           |
 *      Agg(3)   Agg(1)     Sink
 *       |     |
 *     Sink    Sink
 *
 * becomes:
 *
 *              ________T1_______
 *             /                 \
 *        SparseFilter         PartialAgg(2)
 *         /          \               |
 * PartialAgg(3)  PartialAgg(1)       |
 *       |             |              |
 * GRPCSink(1)     GRPCSink(2)    GRPCSink(3)
 * - - - - - - - -  - - - - - - - - - - - -
 * GRPCSource(1) GRPCSource(2)   GRPCSource(3)
 *       |             |              |
 * FinalizeAgg(3) FinalizeAgg(1)   FinalizeAgg(2)
 *       |             |              |
 *      Sink          Sink           Sink
 *
 *
 */

/** Tests the following graph:
 *
 *
 *             ______T1_____
 *             /             \
 *         RandomMap          \
 *           /                 \
 *      MustBePEMMap       NonPartialAgg(2)
 *        /   \                 |
 *     Agg(1) MustBePEMMap     Sink
 *       |     |
 *       |     |
 *       |     |
 *     Sink    Sink
 *
 *
 *
 *             ______T1_____
 *             /             \
 *         RandomMap          \
 *           /                 \
 *      MustBePEMMap(1)      GRPCSink(3))
 *        /        \
 * PartialAgg(1) MustBePEMMap(2)
 *       |            |
 *   GRPCSink(1) GRPCSink(2)
 * - - - - - - - -  - - - - - - - - - - - -
 * GRPCSource(1) GRPCSource(2)   GRPCSource(3)
 *       |             |              |
 * FinalizeAgg(1)      |        NonPartialAgg(2)
 *       |             |              |
 *      Sink          Sink           Sink
 */
TEST_F(SplitterTest, branch_where_children_must_be_on_pem) {
  table_store::schema::Relation relation(
      {types::UINT128, types::TIME64NS, types::INT64, types::DataType::FLOAT64},
      {"upid", "time_", "count", "cpu0"});
  auto mem_src = MakeMemSource("source", relation);
  compiler_state_->relation_map()->emplace("source", relation);
  auto add_func = MakeAddFunc(MakeColumn("cpu0", 0, types::DataType::INT64), MakeInt(1));
  auto random_map = MakeMap(mem_src, {{"cpu++", add_func}},
                            /*keep_input_columns*/ true);

  // Makes a sparse filter.
  auto metadata_fn1 =
      MakeFunc("upid_to_service_name", {MakeColumn("upid", 0, types::DataType::UINT128)});
  metadata_fn1->set_annotations(ExpressionIR::Annotations{MetadataType::SERVICE_NAME});
  auto eq_func = MakeEqualsFunc(metadata_fn1, MakeString("pl/agent1"));
  auto map1 = MakeMap(random_map, {{"equals_service", eq_func}},
                      /*keep_input_columns*/ true);

  // agg1 should partial.
  auto count_col_agg1 = MakeColumn("count", 0, types::DataType::INT64);
  EXPECT_OK(count_col_agg1->SetResolvedType(ValueType::Create(types::INT64, types::ST_NONE)));
  auto count_func1 = MakeCountFunc(MakeColumn("count", 0, types::DataType::INT64));
  auto agg1 = MakeBlockingAgg(map1, {count_col_agg1}, {{"countelms", count_func1}});
  MakeMemSink(agg1, "out2");

  // agg2 should not partial.
  auto agg2_count_fn =
      graph
          ->CreateNode<FuncIR>(
              ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "count_no_partial"},
              std::vector<ExpressionIR*>({MakeColumn("count", 0, types::DataType::INT64)}))
          .ConsumeValueOrDie();
  ASSERT_OK(AddUDAToRegistry("count_no_partial", types::INT64, {types::INT64},
                             /*supports_partial*/ false));
  auto agg2 = MakeBlockingAgg(mem_src, {}, {{"countelms", agg2_count_fn}});
  MakeMemSink(agg2, "out3");

  auto metadata_fn2 =
      MakeFunc("upid_to_pod_name", {MakeColumn("upid", 0, types::DataType::UINT128)});
  metadata_fn2->set_annotations(ExpressionIR::Annotations{MetadataType::POD_NAME});
  auto map2 = MakeMap(
      map1, {{"pod", metadata_fn2}, {"count", MakeColumn("count", 0, types::DataType::INT64)}},
      /*keep_input_columns*/ true);
  MakeMemSink(map2, "out1");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ true);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();
  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();

  auto sources = before_blocking->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(sources.size(), 1);

  // Before should be Memsrc->GRPCSink plan.
  auto new_mem_src = static_cast<MemorySourceIR*>(sources[0]);
  ASSERT_EQ(new_mem_src->Children().size(), 2);
  auto new_random_map = new_mem_src->Children()[0];
  auto grpc_sink3 = static_cast<GRPCSinkIR*>(new_mem_src->Children()[1]);
  ASSERT_MATCH(new_random_map, Map());
  ASSERT_MATCH(grpc_sink3, GRPCSink());

  ASSERT_EQ(new_random_map->Children().size(), 1);
  auto new_map1 = new_random_map->Children()[0];
  ASSERT_MATCH(new_map1, Map());

  ASSERT_EQ(new_map1->Children().size(), 2);
  auto partial_agg1 = new_map1->Children()[0];
  auto new_map2 = new_map1->Children()[1];
  if (Match(partial_agg1, Map())) {
    auto tmp = new_map2;
    new_map2 = partial_agg1;
    partial_agg1 = tmp;
  }
  ASSERT_MATCH(new_map2, Map());
  ASSERT_MATCH(partial_agg1, PartialAgg());

  ASSERT_EQ(partial_agg1->Children().size(), 1);
  ASSERT_EQ(new_map2->Children().size(), 1);

  ASSERT_MATCH(partial_agg1->Children()[0], GRPCSink());
  ASSERT_MATCH(new_map2->Children()[0], GRPCSink());

  auto grpc_sink1 = static_cast<GRPCSinkIR*>(partial_agg1->Children()[0]);
  auto grpc_sink2 = static_cast<GRPCSinkIR*>(new_map2->Children()[0]);

  auto grpc_sources = after_blocking->FindNodesThatMatch(GRPCSourceGroup());
  ASSERT_EQ(grpc_sources.size(), 3);
  GRPCSourceGroupIR* grpc_source1 = nullptr;
  GRPCSourceGroupIR* grpc_source2 = nullptr;
  GRPCSourceGroupIR* grpc_source3 = nullptr;

  for (const auto& s : grpc_sources) {
    auto grpc_src = static_cast<GRPCSourceGroupIR*>(s);
    if (grpc_src->source_id() == grpc_sink1->destination_id()) {
      grpc_source1 = grpc_src;
    }

    if (grpc_src->source_id() == grpc_sink2->destination_id()) {
      grpc_source2 = grpc_src;
    }

    if (grpc_src->source_id() == grpc_sink3->destination_id()) {
      grpc_source3 = grpc_src;
    }
  }

  ASSERT_EQ(grpc_source1->source_id(), grpc_sink1->destination_id());
  ASSERT_EQ(grpc_source2->source_id(), grpc_sink2->destination_id());
  ASSERT_EQ(grpc_source3->source_id(), grpc_sink3->destination_id());

  ASSERT_EQ(grpc_source1->Children().size(), 1);
  auto finalize_agg = grpc_source1->Children()[0];
  EXPECT_MATCH(finalize_agg, FinalizeAgg());
  ASSERT_EQ(finalize_agg->Children().size(), 1);
  EXPECT_MATCH(finalize_agg->Children()[0], MemorySink());

  ASSERT_EQ(grpc_source2->Children().size(), 1);
  EXPECT_MATCH(grpc_source2->Children()[0], MemorySink());

  ASSERT_EQ(grpc_source3->Children().size(), 1);
  EXPECT_MATCH(grpc_source3->Children()[0], BlockingAgg());
  auto new_agg2 = grpc_source3->Children()[0];
  ASSERT_EQ(new_agg2->Children().size(), 1);
  EXPECT_MATCH(new_agg2->Children()[0], MemorySink());
}

/** Tests the following graph:
 *
 *
 *              ______T1_____
 *             /             \
 *         RandomMap1         \
 *           /                 \
 *      MustBePEMMap       NonPartialAgg(2)
 *        /   \                 |
 *     Agg(1) RandomMap2       Sink
 *       |     |
 *     Sink    Sink
 *
 *
 *
 *             ______T1_____
 *             /             \
 *      RandomMap(1)          \
 *           /                 \
 *      MustBePEMMap(1)      GRPCSink(2))
 *          |
 *      GRPCSink(1)
 * - - - - - - - -  - - - - - - - - - - - -
 *     GRPCSource(1)           GRPCSource(3)
 *      /        \                  |
 *  Agg(1)      RandomMap(2)      Agg(2)rest_can
 *     |             |              |
 *    Sink          Sink           Sink
 */
TEST_F(SplitterTest, branch_where_single_node_must_be_on_pem_the_rest_can_recurse) {
  table_store::schema::Relation relation(
      {types::UINT128, types::TIME64NS, types::INT64, types::DataType::FLOAT64},
      {"upid", "time_", "count", "cpu0"});
  auto mem_src = MakeMemSource("source", relation);
  compiler_state_->relation_map()->emplace("source", relation);
  auto add_func = MakeAddFunc(MakeColumn("cpu0", 0, types::DataType::INT64), MakeInt(1));
  auto random_map1 = MakeMap(mem_src, {{"cpu++", add_func}},
                             /*keep_input_columns*/ true);

  // Makes a sparse filter.
  auto metadata_fn1 =
      MakeFunc("upid_to_service_name", {MakeColumn("upid", 0, types::DataType::UINT128)});
  metadata_fn1->set_annotations(ExpressionIR::Annotations{MetadataType::SERVICE_NAME});
  auto eq_func = MakeEqualsFunc(metadata_fn1, MakeString("pl/agent1"));
  auto pem_map1 = MakeMap(random_map1, {{"equals_service", eq_func}},
                          /*keep_input_columns*/ true);

  // agg1 should partial.
  auto count_col_agg1 = MakeColumn("count", 0, types::DataType::INT64);
  EXPECT_OK(count_col_agg1->SetResolvedType(ValueType::Create(types::INT64, types::ST_NONE)));
  auto count_func = MakeCountFunc(MakeColumn("count", 0, types::DataType::INT64));
  auto agg1 = MakeBlockingAgg(pem_map1, {count_col_agg1}, {{"countelms", count_func}});
  MakeMemSink(agg1, "out2");

  // agg2 should not partial.
  auto agg2_count_fn =
      graph
          ->CreateNode<FuncIR>(
              ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "count_no_partial"},
              std::vector<ExpressionIR*>({MakeColumn("count", 0, types::DataType::INT64)}))
          .ConsumeValueOrDie();
  ASSERT_OK(AddUDAToRegistry("count_no_partial", types::INT64, {types::INT64},
                             /*supports_partial*/ false));
  auto agg2 = MakeBlockingAgg(mem_src, {}, {{"countelms", agg2_count_fn}});
  MakeMemSink(agg2, "out3");

  auto add_func2 = MakeAddFunc(MakeColumn("count", 0, types::DataType::INT64), MakeInt(1));
  auto random_map2 = MakeMap(
      pem_map1, {{"count++", add_func2}, {"count", MakeColumn("count", 0, types::DataType::INT64)}},
      /*keep_input_columns*/ true);
  MakeMemSink(random_map2, "out1");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ true);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();
  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();

  auto sources = before_blocking->FindNodesThatMatch(MemorySource());
  ASSERT_EQ(sources.size(), 1);

  // Before should be Memsrc->GRPCSink plan.
  auto new_mem_src = static_cast<MemorySourceIR*>(sources[0]);
  ASSERT_EQ(new_mem_src->Children().size(), 2);
  auto new_random_map1 = new_mem_src->Children()[0];
  auto grpc_sink2 = static_cast<GRPCSinkIR*>(new_mem_src->Children()[1]);
  ASSERT_MATCH(new_random_map1, Map());
  ASSERT_MATCH(grpc_sink2, GRPCSink());

  ASSERT_EQ(new_random_map1->Children().size(), 1);
  auto new_pem_map = new_random_map1->Children()[0];
  ASSERT_MATCH(new_pem_map, Map());

  ASSERT_EQ(new_pem_map->Children().size(), 1);
  ASSERT_MATCH(new_pem_map->Children()[0], GRPCSink());

  auto grpc_sink1 = static_cast<GRPCSinkIR*>(new_pem_map->Children()[0]);

  auto grpc_sources = after_blocking->FindNodesThatMatch(GRPCSourceGroup());
  ASSERT_EQ(grpc_sources.size(), 2);
  GRPCSourceGroupIR* grpc_source1 = nullptr;
  GRPCSourceGroupIR* grpc_source2 = nullptr;

  for (const auto& s : grpc_sources) {
    auto grpc_src = static_cast<GRPCSourceGroupIR*>(s);
    if (grpc_src->source_id() == grpc_sink1->destination_id()) {
      grpc_source1 = grpc_src;
    }

    if (grpc_src->source_id() == grpc_sink2->destination_id()) {
      grpc_source2 = grpc_src;
    }
  }

  ASSERT_EQ(grpc_source1->source_id(), grpc_sink1->destination_id());
  ASSERT_EQ(grpc_source2->source_id(), grpc_sink2->destination_id());

  ASSERT_EQ(grpc_source1->Children().size(), 2);
  auto new_agg1 = grpc_source1->Children()[0];
  auto new_random_map2 = grpc_source1->Children()[1];

  EXPECT_MATCH(new_agg1, BlockingAgg());
  ASSERT_EQ(new_agg1->Children().size(), 1);
  EXPECT_MATCH(new_agg1->Children()[0], MemorySink());

  ASSERT_EQ(new_random_map2->Children().size(), 1);
  EXPECT_MATCH(new_random_map2->Children()[0], MemorySink());

  ASSERT_EQ(grpc_source2->Children().size(), 1);
  auto new_agg2 = grpc_source2->Children()[0];
  EXPECT_MATCH(new_agg2, BlockingAgg());
  ASSERT_EQ(new_agg2->Children().size(), 1);
  EXPECT_MATCH(new_agg2->Children()[0], MemorySink());
}

TEST_F(SplitterTest, schedule_kelvin_only_func_on_kelvin) {
  auto mem_src = MakeMemSource("cpu", cpu_relation);
  auto kelvin_func = MakeFunc("kelvin_only", {});
  auto kelvin_only_map = MakeMap(mem_src, {{"kelvin_only", kelvin_func}},
                                 /*keep_input_columns*/ false);

  table_store::schema::Relation relation1({types::INT64, types::FLOAT64}, {"count", "mean"});

  auto count_func = MakeMeanFuncWithFloatType(MakeColumn("count", 0, types::DataType::INT64));
  auto agg = MakeBlockingAgg(kelvin_only_map, {MakeColumn("count", 0, types::DataType::INT64)},
                             {{"mean", count_func}});
  table_store::schema::Relation relation2({types::INT64, types::FLOAT64}, {"count", "mean"});

  auto sink = MakeMemSink(agg, "out");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();

  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter->SplitKelvinAndAgents(graph.get()).ConsumeValueOrDie();

  auto before_blocking = split_plan->before_blocking.get();
  auto after_blocking = split_plan->after_blocking.get();

  // Verify the resultant graph.
  MemorySourceIR* new_mem_src = GetEquivalentInNewPlan(before_blocking, mem_src);
  ASSERT_EQ(new_mem_src->Children().size(), 1UL);
  OperatorIR* mem_src_child = new_mem_src->Children()[0];
  ASSERT_TRUE(Match(mem_src_child, GRPCSink()))
      << "Expected GRPCSink, got " << mem_src_child->type_string();
  GRPCSinkIR* grpc_sink = static_cast<GRPCSinkIR*>(mem_src_child);

  OperatorIR* new_map = GetEquivalentInNewPlan(after_blocking, kelvin_only_map);
  OperatorIR* map_parent = new_map->parents()[0];
  ASSERT_TRUE(Match(map_parent, GRPCSourceGroup()))
      << "Expected GRPCSourceGroup, got " << map_parent->type_string();
  GRPCSourceGroupIR* grpc_source_group = static_cast<GRPCSourceGroupIR*>(map_parent);
  EXPECT_EQ(grpc_sink->destination_id(), grpc_source_group->source_id());

  OperatorIR* new_agg = GetEquivalentInNewPlan(after_blocking, agg);
  OperatorIR* agg_parent = new_agg->parents()[0];
  EXPECT_EQ(new_map, agg_parent);

  OperatorIR* sink_parent = GetEquivalentInNewPlan(after_blocking, sink)->parents()[0];
  EXPECT_EQ(sink_parent, new_agg);
}

TEST_F(SplitterTest, errors_if_pem_func_on_kelvin) {
  auto mem_src = MakeMemSource("cpu", cpu_relation);
  auto mean_func = MakeMeanFuncWithFloatType(MakeColumn("count", 0, types::DataType::INT64));
  auto agg = MakeBlockingAgg(mem_src, {MakeColumn("count", 0, types::DataType::INT64)},
                             {{"mean", mean_func}});
  auto pem_func = MakeFunc("pem_only", {});
  auto pem_only_map = MakeMap(agg, {{"pem_only", pem_func}}, /*keep_input_columns*/ false);
  MakeMemSink(pem_only_map, "out");

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  auto splitter_or_s = Splitter::Create(compiler_state_.get(), /* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<Splitter> splitter = splitter_or_s.ConsumeValueOrDie();
  auto s = splitter->SplitKelvinAndAgents(graph.get());
  ASSERT_NOT_OK(s);
  EXPECT_THAT(
      s.status(),
      HasCompilerError(
          "UDF 'pem_only' must execute before blocking nodes such as limit, agg, and join."));
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
