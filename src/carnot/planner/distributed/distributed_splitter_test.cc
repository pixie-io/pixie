#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/distributed_planner.h"
#include "src/carnot/planner/distributed/distributed_splitter.h"
#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/udf_exporter/udf_exporter.h"

namespace pl {
namespace carnot {
namespace planner {
namespace distributed {
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

class SplitterTest : public OperatorTests {
 protected:
  void SetUpImpl() override {
    auto rel_map = std::make_unique<RelationMap>();
    cpu_relation = table_store::schema::Relation(
        std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64,
                                      types::DataType::FLOAT64, types::DataType::FLOAT64}),
        std::vector<std::string>({"count", "cpu0", "cpu1", "cpu2"}));
    rel_map->emplace("cpu", cpu_relation);

    compiler_state_ = std::make_unique<CompilerState>(std::move(rel_map), info_.get(), time_now);
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

  std::unique_ptr<CompilerState> compiler_state_;
  std::unique_ptr<RegistryInfo> info_;
  int64_t time_now = 1552607213931245000;
  table_store::schema::Relation cpu_relation;
};

TEST_F(SplitterTest, blocking_agg_test) {
  auto mem_src = MakeMemSource(MakeRelation());
  auto agg = MakeBlockingAgg(mem_src, {MakeColumn("count", 0)},
                             {{"mean", MakeMeanFunc(MakeColumn("count", 0))}});
  auto sink = MakeMemSink(agg, "out");

  auto splitter_or_s = DistributedSplitter::Create(/* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<DistributedSplitter> splitter = splitter_or_s.ConsumeValueOrDie();
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
  auto mem_src = MakeMemSource(MakeRelation());
  auto count_col = MakeColumn("count", 0);
  count_col->ResolveColumnType(types::INT64);
  auto mean_func = MakeMeanFunc(MakeColumn("count", 0));
  mean_func->SetSupportsPartial(true);
  auto agg = MakeBlockingAgg(mem_src, {count_col}, {{"mean", mean_func}});

  table_store::schema::Relation relation({types::INT64, types::FLOAT64}, {"count", "mean"});
  ASSERT_OK(agg->SetRelation(relation));
  MakeMemSink(agg, "out");

  auto splitter_or_s = DistributedSplitter::Create(/* perform_partial_agg */ true);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<DistributedSplitter> splitter = splitter_or_s.ConsumeValueOrDie();
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
  EXPECT_EQ(grpc_sink->relation(),
            Relation({types::INT64, types::STRING}, {"count", "serialized_expressions"}));
  EXPECT_EQ(grpc_source->relation(),
            Relation({types::INT64, types::STRING}, {"count", "serialized_expressions"}));

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
  auto mem_src = MakeMemSource(MakeRelation());
  auto limit = MakeLimit(mem_src, 10);
  EXPECT_OK(limit->SetRelation(mem_src->relation()));
  auto sink = MakeMemSink(limit, "out");
  EXPECT_TRUE(limit->IsRelationInit());

  auto splitter_or_s = DistributedSplitter::Create(/* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<DistributedSplitter> splitter = splitter_or_s.ConsumeValueOrDie();
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

TEST_F(SplitterTest, sink_only_test) {
  auto mem_src = MakeMemSource(MakeRelation());
  auto map1 = MakeMap(mem_src, {{"col0", MakeColumn("col0", 0)}, {"col1", MakeColumn("col1", 0)}});
  auto map2 = MakeMap(map1, {{"col0", MakeColumn("col0", 0)}, {"col1", MakeColumn("col1", 0)}});
  EXPECT_OK(map2->SetRelation(MakeRelation()));
  auto sink = MakeMemSink(map2, "out");

  auto splitter_or_s = DistributedSplitter::Create(/* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<DistributedSplitter> splitter = splitter_or_s.ConsumeValueOrDie();
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
  auto mem_src = MakeMemSource(MakeRelation());
  auto map = MakeMap(mem_src, {{"count", MakeColumn("count", 0)}});
  EXPECT_OK(map->SetRelation(MakeRelation()));
  auto agg = MakeBlockingAgg(map, {MakeColumn("count", 0)},
                             {{"mean", MakeMeanFunc(MakeColumn("count", 0))}});
  auto map2 = MakeMap(agg, {{"count", MakeColumn("count", 0)}});
  MakeMemSink(map2, "out");

  auto splitter_or_s = DistributedSplitter::Create(/* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<DistributedSplitter> splitter = splitter_or_s.ConsumeValueOrDie();
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
  auto mem_src = MakeMemSource(MakeRelation());
  auto agg = MakeBlockingAgg(mem_src, {MakeColumn("count", 0)},
                             {{"mean", MakeMeanFunc(MakeColumn("cpu0", 0))}});
  auto agg2 = MakeBlockingAgg(agg, {MakeColumn("count", 0)},
                              {{"mean2", MakeMeanFunc(MakeColumn("mean", 0))}});
  MakeMemSink(agg2, "out");

  auto splitter_or_s = DistributedSplitter::Create(/* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<DistributedSplitter> splitter = splitter_or_s.ConsumeValueOrDie();
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
  auto mem_src1 = MakeMemSource(MakeRelation());
  auto mem_src2 = MakeMemSource(MakeRelation());
  auto union_op = MakeUnion({mem_src1, mem_src2});
  MakeMemSink(union_op, "out");
  EXPECT_OK(union_op->SetRelationFromParents());

  for (const auto union_parent : union_op->parents()) {
    EXPECT_MATCH(union_parent, MemorySource());
  }

  auto splitter_or_s = DistributedSplitter::Create(/* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<DistributedSplitter> splitter = splitter_or_s.ConsumeValueOrDie();
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
  auto mem_src = MakeMemSource(MakeRelation());
  auto blocking_agg1 = MakeBlockingAgg(mem_src, {MakeColumn("count", 0)},
                                       {{"cpu0_mean", MakeMeanFunc(MakeColumn("cpu0", 0))}});
  MakeMemSink(blocking_agg1, "out1");

  auto blocking_agg2 = MakeBlockingAgg(mem_src, {MakeColumn("count", 0)},
                                       {{"cpu1_mean", MakeMeanFunc(MakeColumn("cpu1", 0))}});
  MakeMemSink(blocking_agg2, "out2");

  EXPECT_EQ(mem_src->Children().size(), 2);

  auto splitter_or_s = DistributedSplitter::Create(/* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<DistributedSplitter> splitter = splitter_or_s.ConsumeValueOrDie();
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
  auto mem_src = MakeMemSource(MakeRelation());
  auto blocking_agg = MakeBlockingAgg(mem_src, {MakeColumn("count", 0)},
                                      {{"cpu0_mean", MakeMeanFunc(MakeColumn("cpu0", 0))}});
  auto join = MakeJoin({mem_src, blocking_agg}, "inner", MakeRelation(),
                       Relation({types::INT64, types::FLOAT64}, {"count", "cpu0_mean"}), {"count"},
                       {"count"});
  MakeMemSink(join, "out");

  EXPECT_EQ(mem_src->Children().size(), 2);

  auto splitter_or_s = DistributedSplitter::Create(/* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<DistributedSplitter> splitter = splitter_or_s.ConsumeValueOrDie();

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
  auto mem_src = MakeMemSource(MakeRelation());
  auto map1 = MakeMap(mem_src, {{"col0", MakeColumn("col0", 0)}, {"col1", MakeColumn("col1", 0)}});
  EXPECT_OK(map1->SetRelation(MakeRelation()));
  auto mem_sink = MakeMemSink(map1, "out");

  auto splitter_or_s = DistributedSplitter::Create(/* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<DistributedSplitter> splitter = splitter_or_s.ConsumeValueOrDie();
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
  auto mem_src1 = MakeMemSource(MakeRelation());
  auto map1 = MakeMap(mem_src1, {{"col0", MakeColumn("col0", 0)}, {"col1", MakeColumn("col1", 0)}});
  EXPECT_OK(map1->SetRelation(MakeRelation()));
  auto mem_sink1 = MakeMemSink(map1, "out");

  auto mem_src2 = MakeMemSource(MakeRelation());
  auto map2 = MakeMap(mem_src2, {{"col0", MakeColumn("col0", 0)}, {"col1", MakeColumn("col1", 0)}});
  EXPECT_OK(map2->SetRelation(MakeRelation()));
  auto mem_sink2 = MakeMemSink(map2, "out");

  auto splitter_or_s = DistributedSplitter::Create(/* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<DistributedSplitter> splitter = splitter_or_s.ConsumeValueOrDie();
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

  auto splitter_or_s = DistributedSplitter::Create(/* perform_partial_agg */ false);
  ASSERT_OK(splitter_or_s);
  std::unique_ptr<DistributedSplitter> splitter = splitter_or_s.ConsumeValueOrDie();
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

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace pl
