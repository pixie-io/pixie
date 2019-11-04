#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/distributed_planner.h"
#include "src/carnot/compiler/distributed_splitter.h"
#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/metadata_handler.h"
#include "src/carnot/compiler/rule_mock.h"
#include "src/carnot/compiler/rules.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace distributed {
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

class BlockingOperatorGRPCBridgeRuleTest : public OperatorTests {
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
  std::unique_ptr<CompilerState> compiler_state_;
  std::unique_ptr<RegistryInfo> info_;
  int64_t time_now = 1552607213931245000;
  table_store::schema::Relation cpu_relation;
};

TEST_F(BlockingOperatorGRPCBridgeRuleTest, blocking_agg_test) {
  auto mem_src = MakeMemSource(MakeRelation());
  auto agg = MakeBlockingAgg(mem_src, {MakeColumn("count", 0)},
                             {{"mean", MakeMeanFunc(MakeColumn("count", 0))}});
  auto sink = MakeMemSink(agg, "out");

  BlockingOperatorGRPCBridgeRule blocking_op_grpc_bridge_rule(compiler_state_.get());
  auto result = blocking_op_grpc_bridge_rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  // Verify the resultant graph.
  plan::DAG dag = graph->dag();
  std::vector<OperatorIR*> op_children;
  for (int64_t d : dag.DependenciesOf(mem_src->id())) {
    auto ir_node = graph->Get(d);
    if (ir_node->IsOperator()) {
      op_children.push_back(static_cast<OperatorIR*>(ir_node));
    }
  }
  ASSERT_EQ(op_children.size(), 1UL);
  OperatorIR* mem_src_child = op_children[0];
  ASSERT_TRUE(Match(mem_src_child, GRPCSink()))
      << "Expected GRPCSink, got " << mem_src_child->type_string();
  GRPCSinkIR* grpc_sink = static_cast<GRPCSinkIR*>(mem_src_child);

  OperatorIR* agg_parent = agg->parents()[0];
  ASSERT_TRUE(Match(agg_parent, GRPCSourceGroup()))
      << "Expected GRPCSourceGroup, got " << agg_parent->type_string();
  GRPCSourceGroupIR* grpc_source_group = static_cast<GRPCSourceGroupIR*>(agg_parent);

  EXPECT_EQ(grpc_sink->destination_id(), grpc_source_group->source_id());

  OperatorIR* sink_parent = sink->parents()[0];
  EXPECT_TRUE(Match(sink_parent, BlockingAgg()));
}

TEST_F(BlockingOperatorGRPCBridgeRuleTest, sink_only_test) {
  auto mem_src = MakeMemSource(MakeRelation());
  auto map1 = MakeMap(mem_src, {{"col0", MakeColumn("col0", 0)}, {"col1", MakeColumn("col1", 0)}});
  auto map2 = MakeMap(map1, {{"col0", MakeColumn("col0", 0)}, {"col1", MakeColumn("col1", 0)}});
  EXPECT_OK(map2->SetRelation(MakeRelation()));
  auto sink = MakeMemSink(map2, "out");

  BlockingOperatorGRPCBridgeRule blocking_op_grpc_bridge_rule(compiler_state_.get());
  auto result = blocking_op_grpc_bridge_rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  // Verify the resultant graph.
  std::vector<OperatorIR*> op_children = map2->Children();
  ASSERT_EQ(op_children.size(), 1UL);
  OperatorIR* op_child = op_children[0];
  ASSERT_TRUE(Match(op_child, GRPCSink())) << "Expected GRPCSink, got " << op_child->type_string();
  GRPCSinkIR* grpc_sink = static_cast<GRPCSinkIR*>(op_child);

  OperatorIR* sink_parent = sink->parents()[0];
  ASSERT_TRUE(Match(sink_parent, GRPCSourceGroup()))
      << "Expected GRPCSourceGroup, got " << sink_parent->type_string();
  GRPCSourceGroupIR* grpc_source_group = static_cast<GRPCSourceGroupIR*>(sink_parent);

  EXPECT_EQ(grpc_sink->destination_id(), grpc_source_group->source_id());
}

// Test to see whether splitting works when sandwiched between two separate ops.
TEST_F(BlockingOperatorGRPCBridgeRuleTest, sandwich_test) {
  auto mem_src = MakeMemSource(MakeRelation());
  auto map = MakeMap(mem_src, {{"count", MakeColumn("count", 0)}});
  EXPECT_OK(map->SetRelation(MakeRelation()));
  auto agg = MakeBlockingAgg(map, {MakeColumn("count", 0)},
                             {{"mean", MakeMeanFunc(MakeColumn("count", 0))}});
  auto map2 = MakeMap(agg, {{"count", MakeColumn("count", 0)}});
  MakeMemSink(map2, "out");

  BlockingOperatorGRPCBridgeRule blocking_op_grpc_bridge_rule(compiler_state_.get());
  auto result = blocking_op_grpc_bridge_rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  // Verify the resultant graph.
  plan::DAG dag = graph->dag();
  std::vector<OperatorIR*> op_children;
  for (int64_t d : dag.DependenciesOf(map->id())) {
    auto ir_node = graph->Get(d);
    if (ir_node->IsOperator()) {
      op_children.push_back(static_cast<OperatorIR*>(ir_node));
    }
  }
  ASSERT_EQ(op_children.size(), 1UL);
  OperatorIR* map_src_child = op_children[0];
  ASSERT_TRUE(Match(map_src_child, GRPCSink()))
      << "Expected GRPCSink, got " << map_src_child->type_string();
  GRPCSinkIR* grpc_sink = static_cast<GRPCSinkIR*>(map_src_child);

  OperatorIR* agg_parent = agg->parents()[0];
  ASSERT_TRUE(Match(agg_parent, GRPCSourceGroup()))
      << "Expected GRPCSourceGroup, got " << agg_parent->type_string();
  GRPCSourceGroupIR* grpc_source_group = static_cast<GRPCSourceGroupIR*>(agg_parent);

  EXPECT_EQ(grpc_sink->destination_id(), grpc_source_group->source_id());
}

TEST_F(BlockingOperatorGRPCBridgeRuleTest, first_blocking_node_test) {
  auto mem_src = MakeMemSource(MakeRelation());
  auto agg = MakeBlockingAgg(mem_src, {MakeColumn("count", 0)},
                             {{"mean", MakeMeanFunc(MakeColumn("cpu0", 0))}});
  auto agg2 = MakeBlockingAgg(agg, {MakeColumn("count", 0)},
                              {{"mean2", MakeMeanFunc(MakeColumn("mean", 0))}});
  MakeMemSink(agg2, "out");

  BlockingOperatorGRPCBridgeRule blocking_op_grpc_bridge_rule(compiler_state_.get());
  auto result = blocking_op_grpc_bridge_rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  // Verify the resultant graph.
  plan::DAG dag = graph->dag();
  std::vector<OperatorIR*> op_children;
  for (int64_t d : dag.DependenciesOf(mem_src->id())) {
    auto ir_node = graph->Get(d);
    if (ir_node->IsOperator()) {
      op_children.push_back(static_cast<OperatorIR*>(ir_node));
    }
  }
  ASSERT_EQ(op_children.size(), 1UL);
  OperatorIR* mem_src_child = op_children[0];
  ASSERT_TRUE(Match(mem_src_child, GRPCSink()))
      << "Expected GRPCSink, got " << mem_src_child->type_string();
  GRPCSinkIR* grpc_sink = static_cast<GRPCSinkIR*>(mem_src_child);

  OperatorIR* agg_parent = agg->parents()[0];
  ASSERT_TRUE(Match(agg_parent, GRPCSourceGroup()))
      << "Expected GRPCSourceGroup, got " << agg_parent->type_string();
  GRPCSourceGroupIR* grpc_source_group = static_cast<GRPCSourceGroupIR*>(agg_parent);

  EXPECT_EQ(grpc_sink->destination_id(), grpc_source_group->source_id());

  OperatorIR* agg2_parent = agg2->parents()[0];
  ASSERT_TRUE(Match(agg2_parent, BlockingAgg()))
      << "Expected BlockingAgg, got " << agg2_parent->type_string();
}

TEST_F(BlockingOperatorGRPCBridgeRuleTest, no_blocking_node_test) {
  auto mem_src = MakeMemSource(MakeRelation());
  auto map1 = MakeMap(mem_src, {{"col0", MakeColumn("col0", 0)}, {"col1", MakeColumn("col1", 0)}});
  auto map2 = MakeMap(map1, {{"col0", MakeColumn("col0", 0)}, {"col1", MakeColumn("col1", 0)}});
  EXPECT_OK(map2->SetRelation(MakeRelation()));

  BlockingOperatorGRPCBridgeRule blocking_op_grpc_bridge_rule(compiler_state_.get());
  auto result = blocking_op_grpc_bridge_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ConsumeValueOrDie());
}

// This test shows that if there is more than one child of an op, the rule will evaluate to true.
// This looks like weird behavior, but we have an upcoming analyzer rule with PL-831 to error
// out whenever the logical plan has an operator that doesn't have a sink with it.
TEST_F(BlockingOperatorGRPCBridgeRuleTest, multiple_children_no_blocking_node_test) {
  auto mem_src = MakeMemSource(MakeRelation());
  auto map1 = MakeMap(mem_src, {{"col0", MakeColumn("col0", 0)}, {"col1", MakeColumn("col1", 0)}});
  EXPECT_OK(map1->SetRelation(MakeRelation()));
  MakeMemSink(map1, "name");

  // Map2 does not have a child.
  auto map2 = MakeMap(map1, {{"col0", MakeColumn("col0", 0)}, {"col1", MakeColumn("col1", 0)}});
  EXPECT_OK(map2->SetRelation(MakeRelation()));

  BlockingOperatorGRPCBridgeRule blocking_op_grpc_bridge_rule(compiler_state_.get());
  auto result = blocking_op_grpc_bridge_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());
}

// Test feeding into unions.
TEST_F(BlockingOperatorGRPCBridgeRuleTest, union_operator) {
  auto mem_src1 = MakeMemSource(MakeRelation());
  auto mem_src2 = MakeMemSource(MakeRelation());
  auto union_op = MakeUnion({mem_src1, mem_src2});
  MakeMemSink(union_op, "out");
  EXPECT_OK(union_op->SetRelationFromParents());

  for (const auto union_parent : union_op->parents()) {
    EXPECT_TRUE(Match(union_parent, MemorySource()));
  }

  BlockingOperatorGRPCBridgeRule blocking_op_grpc_bridge_rule(compiler_state_.get());
  auto result = blocking_op_grpc_bridge_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  std::vector<int64_t> source_group_ids;
  for (auto union_parent : union_op->parents()) {
    ASSERT_TRUE(Match(union_parent, GRPCSourceGroup()))
        << absl::Substitute("Expected node $0 to be GRPCSourceGroup.", union_parent->DebugString());
    source_group_ids.push_back(static_cast<GRPCSourceGroupIR*>(union_parent)->source_id());
  }

  std::vector<int64_t> sink_ids;
  auto children1 = mem_src1->Children();
  ASSERT_EQ(children1.size(), 1);
  ASSERT_TRUE(Match(children1[0], GRPCSink()))
      << absl::Substitute("Expected node $0 to be GRPCSink.", children1[0]->DebugString());

  sink_ids.push_back(static_cast<GRPCSinkIR*>(children1[0])->destination_id());

  auto children2 = mem_src2->Children();
  ASSERT_EQ(children2.size(), 1);
  EXPECT_TRUE(Match(children2[0], GRPCSink()))
      << absl::Substitute("Expected node $0 to be GRPCSink.", children2[0]->DebugString());
  sink_ids.push_back(static_cast<GRPCSinkIR*>(children2[0])->destination_id());

  EXPECT_THAT(source_group_ids, UnorderedElementsAreArray(sink_ids));
}

// Two blocking children of one source operator
// TODO(philkuz) (PL-846) optimize this case to only have one GRPCBridge.
/** Tests the following graph.
 *    T1
 *   /  \
 * Agg1   Agg2
 */
TEST_F(BlockingOperatorGRPCBridgeRuleTest, two_blocking_children) {
  auto mem_src = MakeMemSource(MakeRelation());
  auto blocking_agg1 = MakeBlockingAgg(mem_src, {MakeColumn("count", 0)},
                                       {{"cpu0_mean", MakeMeanFunc(MakeColumn("cpu0", 0))}});
  MakeMemSink(blocking_agg1, "out1");

  auto blocking_agg2 = MakeBlockingAgg(mem_src, {MakeColumn("count", 0)},
                                       {{"cpu1_mean", MakeMeanFunc(MakeColumn("cpu1", 0))}});
  MakeMemSink(blocking_agg2, "out2");

  EXPECT_EQ(mem_src->Children().size(), 2);

  BlockingOperatorGRPCBridgeRule blocking_op_grpc_bridge_rule(compiler_state_.get());
  auto result = blocking_op_grpc_bridge_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  ASSERT_EQ(blocking_agg1->parents().size(), 1);
  EXPECT_TRUE(Match(blocking_agg1->parents()[0], GRPCSourceGroup()));
  auto grpc_source1 = static_cast<GRPCSourceGroupIR*>(blocking_agg1->parents()[0]);

  ASSERT_EQ(blocking_agg2->parents().size(), 1);
  EXPECT_TRUE(Match(blocking_agg2->parents()[0], GRPCSourceGroup()));
  auto grpc_source2 = static_cast<GRPCSourceGroupIR*>(blocking_agg2->parents()[0]);

  // TODO(philkuz) (PL-846) replace the following with the commented out code.
  EXPECT_NE(grpc_source1->source_id(), grpc_source2->source_id());
  auto source_children = mem_src->Children();
  ASSERT_EQ(source_children.size(), 2);
  ASSERT_TRUE(Match(source_children[0], GRPCSink()));
  ASSERT_TRUE(Match(source_children[1], GRPCSink()));

  auto grpc_sink1 = static_cast<GRPCSinkIR*>(source_children[0]);
  auto grpc_sink2 = static_cast<GRPCSinkIR*>(source_children[1]);

  EXPECT_THAT(std::vector<int64_t>({grpc_source1->source_id(), grpc_source2->source_id()}),
              UnorderedElementsAre(grpc_sink1->destination_id(), grpc_sink2->destination_id()));

  // TODO(philkuz) (PL-846) uncomment with this issue.
  // EXPECT_EQ(grpc_source1->source_id(), grpc_source2->source_id());
  // EXPECT_EQ(grpc_source1, grpc_source2);
  // auto source_children = mem_src->Children();
  // ASSERT_EQ(source_children.size(), 1);
  // ASSERT_TRUE(Match(source_children[0], GRPCSink()));

  // auto grpc_sink1 = static_cast<GRPCSinkIR*>(source_children[0]);

  // EXPECT_EQ(grpc_source1->source_id(), grpc_sink1->destination_id());
}

// TODO(philkuz) (PL-751) Add this test when we have a join operator in play
// TODO(philkuz) (PL-846) optimize this case to only have one GRPCBridge.
/** Tests the following graph.
 *    T1
 *   /  \
 * Agg   \
 *   \   /
 *    Join
 */
TEST_F(BlockingOperatorGRPCBridgeRuleTest, DISABLED_agg_join_children) {
  auto mem_src = MakeMemSource(MakeRelation());
  auto blocking_agg = MakeBlockingAgg(mem_src, {MakeColumn("count", 0)},
                                      {{"cpu0_mean", MakeMeanFunc(MakeColumn("cpu0", 0))}});

  // auto join = MakeJoin(mem_src, blocking_agg, {{MakeColumn("count", 0), MakeColumn("count",
  // 1)}});
  // TODO(philkuz) remove this call when we have a join operator and uncomment the above.
  auto join = MakeMap(blocking_agg, {{"count", MakeColumn("count", 0)}});
  MakeMemSink(join, "out");

  EXPECT_EQ(mem_src->Children().size(), 2);

  BlockingOperatorGRPCBridgeRule blocking_op_grpc_bridge_rule(compiler_state_.get());
  auto result = blocking_op_grpc_bridge_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  ASSERT_EQ(blocking_agg->parents().size(), 1);
  ASSERT_TRUE(Match(blocking_agg->parents()[0], GRPCSourceGroup()));
  auto blocking_agg_parent = static_cast<GRPCSourceGroupIR*>(blocking_agg->parents()[0]);

  ASSERT_EQ(join->parents().size(), 2);
  // Parent 1 should be the GRPCSourceGroup
  ASSERT_TRUE(Match(join->parents()[0], GRPCSourceGroup()));
  auto join_parent = static_cast<GRPCSourceGroupIR*>(join->parents()[0]);

  // TODO(philkuz) Replace the following with the commented out code with (PL-846).
  EXPECT_NE(join_parent->source_id(), blocking_agg_parent->source_id());
  auto source_children = mem_src->Children();
  ASSERT_EQ(source_children.size(), 2);
  ASSERT_TRUE(Match(source_children[0], GRPCSink()));
  ASSERT_TRUE(Match(source_children[1], GRPCSink()));

  auto grpc_sink1 = static_cast<GRPCSinkIR*>(source_children[0]);
  auto grpc_sink2 = static_cast<GRPCSinkIR*>(source_children[1]);

  EXPECT_THAT(std::vector<int64_t>({join_parent->source_id(), blocking_agg_parent->source_id()}),
              UnorderedElementsAre(grpc_sink1->destination_id(), grpc_sink2->destination_id()));

  // TODO(philkuz) Uncomment with (PL-846)
  // EXPECT_EQ(join_parent->source_id(), blocking_agg_parent->source_id());
  // EXPECT_EQ(join_parent, blocking_agg_parent);
  // EXPECT_EQ(join_parent.Children().size(), 2);
  // auto source_children = mem_src->Children();
  // ASSERT_EQ(source_children.size(), 1);
  // ASSERT_TRUE(Match(source_children[0], GRPCSink()));

  // auto grpc_sink = static_cast<GRPCSinkIR*>(source_children[0]);

  // EXPECT_EQ(join_parent->source_id(), grpc_sink->destination_id());
}

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
    // TODO(philkuz) with multiple parents support check to see whether we can get
    // check whether either has a parent. Maybe we just pass in an index?
    ASSERT_EQ(op->parents().size(), 1);
    OperatorIR* sink_parent = op->parents()[0];
    EXPECT_EQ(sink_parent->type(), IRNodeType::kGRPCSourceGroup);
  }
  std::unique_ptr<CompilerState> compiler_state_;
  std::unique_ptr<RegistryInfo> info_;
  int64_t time_now = 1552607213931245000;
  table_store::schema::Relation cpu_relation;
};

TEST_F(SplitterTest, simple_split_test) {
  auto mem_src = MakeMemSource(MakeRelation());
  auto map1 = MakeMap(mem_src, {{"col0", MakeColumn("col0", 0)}, {"col1", MakeColumn("col1", 0)}});
  EXPECT_OK(map1->SetRelation(MakeRelation()));
  auto mem_sink = MakeMemSink(map1, "out");

  DistributedSplitter splitter(compiler_state_.get());
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter.SplitAtBlockingNode(graph.get()).ConsumeValueOrDie();
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

  DistributedSplitter splitter(compiler_state_.get());
  std::unique_ptr<BlockingSplitPlan> split_plan =
      splitter.SplitAtBlockingNode(graph.get()).ConsumeValueOrDie();

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

}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
