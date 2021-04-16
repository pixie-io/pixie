#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/tablet_rules.h"
namespace px {
namespace carnot {
namespace planner {
namespace distributed {
using table_store::schema::Relation;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

class TabletSourceConversionRuleTest : public OperatorTests {
 protected:
  void SetUpImpl() override {
    relation0_ =
        Relation({types::TIME64NS, types::UINT128, types::INT64}, {"time_", "upid", "cycles"});
    relation1_ = Relation({types::TIME64NS, types::INT64}, {"time_", "read_bytes"});
  }

  Relation relation0_;
  Relation relation1_;
};

constexpr char kCarnotInfo[] = R"proto(
query_broker_address: "carnot"
has_data_store: true
processes_data: true
accepts_remote_sources: false
table_info {
  table: "cpu_table"
  tabletization_key: "upid"
  tablets: "1"
  tablets: "2"
}
table_info {
  table: "network"
}
)proto";

TEST_F(TabletSourceConversionRuleTest, simple_test) {
  distributedpb::CarnotInfo carnot_info;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kCarnotInfo, &carnot_info));
  // This table has tablet keys so it should be transformed as appropriate.
  auto mem_src0 = MakeMemSource("cpu_table", relation0_);
  auto mem_sink0 = MakeMemSink(mem_src0, "out");

  // This table does not have tablet keys so ti should not be transformed.
  auto mem_src1 = MakeMemSource("network", relation1_);
  auto mem_sink1 = MakeMemSink(mem_src1, "out");

  TabletSourceConversionRule tabletization_rule(carnot_info);
  auto result = tabletization_rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  // mem_sink0's source should change to the tablet_source_group
  OperatorIR* sink0_parent = mem_sink0->parents()[0];
  EXPECT_NE(sink0_parent, mem_src0);
  ASSERT_EQ(mem_sink0->parents()[0]->type(), IRNodeType::kTabletSourceGroup);

  TabletSourceGroupIR* tablet_source_group =
      static_cast<TabletSourceGroupIR*>(mem_sink0->parents()[0]);

  EXPECT_THAT(tablet_source_group->tablets(), ElementsAre("1", "2"));

  EXPECT_EQ(tablet_source_group->ReplacedMemorySource(), mem_src0);
  EXPECT_THAT(tablet_source_group->Children(), ElementsAre(mem_sink0));

  EXPECT_TRUE(tablet_source_group->IsRelationInit());

  // Make sure that mem_src0 is properly removed from the graph, but not the pool yet.
  EXPECT_EQ(mem_src0->Children().size(), 0);
  EXPECT_TRUE(graph->HasNode(mem_src0->id()));

  // mem_sink1's source should not change.
  EXPECT_EQ(mem_sink1->parents()[0], mem_src1);
}

using MemorySourceTabletRuleTest = TabletSourceConversionRuleTest;
TEST_F(MemorySourceTabletRuleTest, tablet_source_group_unions) {
  Relation relation({types::UINT128, types::INT64, types::STRING}, {"upid", "cpu0", "name"});
  auto mem_src = MakeMemSource("table", relation);
  std::vector<types::TabletID> in_tablet_values = {"tablet1", "tablet2"};

  TabletSourceGroupIR* tablet_source_group =
      MakeTabletSourceGroup(mem_src, in_tablet_values, "upid");
  MemorySinkIR* mem_sink = MakeMemSink(tablet_source_group, "out");

  int64_t tablet_source_group_id = tablet_source_group->id();
  int64_t mem_src_id = mem_src->id();

  MemorySourceTabletRule rule;
  auto result = rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  // Check to see that the group is no longer available
  EXPECT_FALSE(graph->HasNode(tablet_source_group_id));
  EXPECT_FALSE(graph->HasNode(mem_src_id));

  // Check to see that mem_sink1 has a new parent that is a union.
  ASSERT_EQ(mem_sink->parents().size(), 1UL);
  OperatorIR* mem_sink_parent = mem_sink->parents()[0];
  ASSERT_EQ(mem_sink_parent->type(), IRNodeType::kUnion) << mem_sink_parent->type_string();

  // Check to see that the union's parents are memory_sources.
  std::vector<types::TabletID> tablet_values;
  UnionIR* union_op = static_cast<UnionIR*>(mem_sink_parent);

  EXPECT_TRUE(union_op->HasColumnMappings());
  EXPECT_TRUE(union_op->IsRelationInit());

  for (auto* union_op_parent : union_op->parents()) {
    ASSERT_EQ(union_op_parent->type(), IRNodeType::kMemorySource) << union_op_parent->type_string();
    auto mem_source = static_cast<MemorySourceIR*>(union_op_parent);
    ASSERT_TRUE(mem_source->HasTablet()) << union_op_parent->type_string();
    tablet_values.push_back(mem_source->tablet_value());
    EXPECT_TRUE(mem_source->IsRelationInit());
    EXPECT_FALSE(mem_source->IsTimeSet());
  }

  EXPECT_THAT(tablet_values, ElementsAreArray(in_tablet_values));
}

TEST_F(MemorySourceTabletRuleTest, tablet_source_group_no_union) {
  Relation relation({types::UINT128, types::INT64, types::STRING}, {"upid", "cpu0", "name"});
  auto mem_src = MakeMemSource("table", relation);
  std::vector<types::TabletID> tablet_values = {"tablet1"};

  TabletSourceGroupIR* tablet_source_group = MakeTabletSourceGroup(mem_src, tablet_values, "upid");
  MemorySinkIR* mem_sink = MakeMemSink(tablet_source_group, "out");

  int64_t tablet_source_group_id = tablet_source_group->id();
  int64_t mem_src_id = mem_src->id();

  MemorySourceTabletRule rule;
  auto result = rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  // Check to see that the group is no longer available
  EXPECT_FALSE(graph->HasNode(tablet_source_group_id));
  EXPECT_FALSE(graph->HasNode(mem_src_id));

  // Check to see that mem_sink1 has a new parent that is a union.
  ASSERT_EQ(mem_sink->parents().size(), 1UL);
  OperatorIR* mem_sink_parent = mem_sink->parents()[0];
  ASSERT_EQ(mem_sink_parent->type(), IRNodeType::kMemorySource) << mem_sink_parent->type_string();

  // Check to see that the union's parents are memory_sources.
  MemorySourceIR* new_mem_src = static_cast<MemorySourceIR*>(mem_sink_parent);
  ASSERT_TRUE(new_mem_src->HasTablet());
  EXPECT_TRUE(new_mem_src->IsRelationInit());
  EXPECT_FALSE(new_mem_src->IsTimeSet());
  EXPECT_THAT(tablet_values, Contains(new_mem_src->tablet_value()));
}

TEST_F(MemorySourceTabletRuleTest, tablet_source_group_union_tabletization_key_filter) {
  Relation relation({types::UINT128, types::INT64, types::STRING}, {"upid", "cpu0", "name"});
  auto mem_src = MakeMemSource("table", relation);
  std::vector<types::TabletID> tablet_values = {"tablet1", "tablet2"};

  TabletSourceGroupIR* tablet_source_group = MakeTabletSourceGroup(mem_src, tablet_values, "upid");
  auto column = MakeColumn("upid", 0);
  auto tablet = MakeString("tablet2");

  FuncIR* filter_expr = MakeEqualsFunc(column, tablet);
  filter_expr->SetOutputDataType(types::BOOLEAN);
  FilterIR* filter = MakeFilter(tablet_source_group, filter_expr);
  MemorySinkIR* mem_sink = MakeMemSink(filter, "out");

  int64_t tablet_source_group_id = tablet_source_group->id();
  int64_t filter_id = filter->id();
  int64_t filter_expr_id = filter_expr->id();
  int64_t column_id = column->id();
  int64_t tablet_id = tablet->id();
  int64_t mem_src_id = mem_src->id();

  EXPECT_THAT(graph, HasEdge(tablet_source_group, filter));
  EXPECT_THAT(graph, HasEdge(filter, filter_expr));
  EXPECT_THAT(graph, HasEdge(filter_expr, column));
  EXPECT_THAT(graph, HasEdge(filter_expr, tablet));
  EXPECT_THAT(graph, HasEdge(filter, mem_sink));
  EXPECT_THAT(graph, Not(HasEdge(tablet_source_group, mem_src)));
  EXPECT_THAT(graph, Not(HasEdge(mem_src, mem_sink)));

  MemorySourceTabletRule rule;
  auto result = rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  // Graph is cleaned up.
  EXPECT_THAT(graph, Not(HasEdge(tablet_source_group_id, filter_id)));
  EXPECT_THAT(graph, Not(HasEdge(filter_id, filter_expr_id)));
  EXPECT_THAT(graph, Not(HasEdge(filter_expr_id, column_id)));
  EXPECT_THAT(graph, Not(HasEdge(filter_expr_id, tablet_id)));
  EXPECT_THAT(graph, Not(HasEdge(filter_id, mem_sink->id())));

  // Check to see that the group is no longer available
  EXPECT_FALSE(graph->HasNode(tablet_source_group_id));
  EXPECT_FALSE(graph->HasNode(filter_id));
  // Tablet source group produces a new memory source.
  EXPECT_FALSE(graph->HasNode(mem_src_id));
  EXPECT_TRUE(graph->HasNode(mem_sink->id()));

  // Check to see that mem_sink1 has a new parent that is a union.
  ASSERT_EQ(mem_sink->parents().size(), 1UL);
  OperatorIR* mem_sink_parent = mem_sink->parents()[0];
  ASSERT_EQ(mem_sink_parent->type(), IRNodeType::kMemorySource) << mem_sink_parent->type_string();

  // Check to see that the union's parents are memory_sourcesj
  MemorySourceIR* new_mem_src = static_cast<MemorySourceIR*>(mem_sink_parent);
  ASSERT_TRUE(new_mem_src->HasTablet());
  EXPECT_TRUE(new_mem_src->IsRelationInit());
  EXPECT_FALSE(new_mem_src->IsTimeSet());
  EXPECT_EQ(new_mem_src->tablet_value(), "tablet2");

  // Confirm these are the only two elements left in the graph.
  EXPECT_THAT(graph->dag().TopologicalSort(), ElementsAre(new_mem_src->id(), mem_sink->id()));
}

TEST_F(MemorySourceTabletRuleTest, tablet_source_group_union_tabletization_key_filter_and) {
  Relation relation({types::UINT128, types::INT64, types::STRING}, {"upid", "cpu0", "name"});
  auto mem_src = MakeMemSource("table", relation);

  std::vector<types::TabletID> in_tablet_values = {"tablet1", "tablet2", "tablet3"};

  TabletSourceGroupIR* tablet_source_group =
      MakeTabletSourceGroup(mem_src, in_tablet_values, "upid");
  auto tablet1 = MakeString("tablet2");
  auto tablet2 = MakeString("tablet3");

  auto column1 = MakeColumn("upid", 0);
  auto column2 = MakeColumn("upid", 0);
  auto equals1 = MakeEqualsFunc(column1, tablet1);
  auto equals2 = MakeEqualsFunc(column2, tablet2);
  auto filter_expr = MakeAndFunc(equals1, equals2);
  filter_expr->SetOutputDataType(types::BOOLEAN);
  FilterIR* filter = MakeFilter(tablet_source_group, filter_expr);

  MemorySinkIR* mem_sink = MakeMemSink(filter, "out");

  int64_t mem_src_id = mem_src->id();
  int64_t tablet_source_group_id = tablet_source_group->id();
  int64_t filter_id = filter->id();

  EXPECT_THAT(graph, HasEdge(tablet_source_group, filter));
  EXPECT_THAT(graph, HasEdge(filter, mem_sink));
  EXPECT_THAT(graph, HasEdge(filter, filter_expr));
  EXPECT_THAT(graph, HasEdge(filter_expr, equals1));
  EXPECT_THAT(graph, HasEdge(filter_expr, equals2));
  EXPECT_THAT(graph, HasEdge(equals1, column1));
  EXPECT_THAT(graph, HasEdge(equals1, tablet1));
  EXPECT_THAT(graph, HasEdge(equals2, column2));
  EXPECT_THAT(graph, HasEdge(equals2, tablet2));
  EXPECT_THAT(graph, Not(HasEdge(mem_src, mem_sink)));

  MemorySourceTabletRule rule;
  auto result = rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  // Check to see that the group is no longer available
  EXPECT_FALSE(graph->HasNode(tablet_source_group_id));
  EXPECT_FALSE(graph->HasNode(filter_id));
  EXPECT_FALSE(graph->HasNode(mem_src_id));

  // Check to see that mem_sink1 has a new parent that is a union.
  ASSERT_EQ(mem_sink->parents().size(), 1UL);
  OperatorIR* mem_sink_parent = mem_sink->parents()[0];
  ASSERT_EQ(mem_sink_parent->type(), IRNodeType::kUnion) << mem_sink_parent->type_string();

  // Check to see that the union's parents are memory_sources.
  std::vector<types::TabletID> tablet_values;
  UnionIR* union_op = static_cast<UnionIR*>(mem_sink_parent);

  EXPECT_TRUE(union_op->HasColumnMappings());
  EXPECT_TRUE(union_op->IsRelationInit());

  for (auto* union_op_parent : union_op->parents()) {
    ASSERT_EQ(union_op_parent->type(), IRNodeType::kMemorySource) << union_op_parent->type_string();
    auto mem_source = static_cast<MemorySourceIR*>(union_op_parent);
    ASSERT_TRUE(mem_source->HasTablet()) << union_op_parent->type_string();
    tablet_values.push_back(mem_source->tablet_value());
    EXPECT_TRUE(mem_source->IsRelationInit());
    EXPECT_FALSE(mem_source->IsTimeSet());
  }

  EXPECT_THAT(tablet_values, ElementsAre("tablet2", "tablet3"));
}

TEST_F(MemorySourceTabletRuleTest, tablet_source_group_filter_does_nothing) {
  Relation relation({types::UINT128, types::INT64, types::STRING}, {"upid", "cpu0", "name"});
  auto mem_src = MakeMemSource("table", relation);
  std::vector<types::TabletID> in_tablet_values = {"tablet1", "tablet2", "tablet3"};

  TabletSourceGroupIR* tablet_source_group =
      MakeTabletSourceGroup(mem_src, in_tablet_values, "upid");
  auto column = MakeColumn("name", 0);
  auto tablet_value = MakeString("blah");

  auto filter_expr = MakeEqualsFunc(column, tablet_value);
  filter_expr->SetOutputDataType(types::BOOLEAN);
  FilterIR* filter = MakeFilter(tablet_source_group, filter_expr);
  MemorySinkIR* mem_sink = MakeMemSink(filter, "out");

  int64_t tablet_source_group_id = tablet_source_group->id();
  int64_t filter_id = filter->id();

  EXPECT_THAT(graph, HasEdge(tablet_source_group, filter));
  EXPECT_THAT(graph, HasEdge(filter, mem_sink));
  EXPECT_THAT(graph, HasEdge(filter, filter_expr));
  EXPECT_THAT(graph, Not(HasEdge(mem_src, mem_sink)));

  EXPECT_THAT(graph->dag().nodes(),
              UnorderedElementsAre(mem_src->id(), tablet_source_group_id, filter_id, mem_sink->id(),
                                   filter_expr->id(), column->id(), tablet_value->id()));

  MemorySourceTabletRule rule;
  auto result = rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  // Check to see that the group is no longer available
  EXPECT_FALSE(graph->HasNode(tablet_source_group_id));
  EXPECT_TRUE(graph->HasNode(filter_id));

  // Check to see that mem_sink1 has a new parent that is a union.
  ASSERT_EQ(mem_sink->parents().size(), 1UL);
  OperatorIR* mem_sink_parent = mem_sink->parents()[0];
  ASSERT_EQ(mem_sink_parent->type(), IRNodeType::kFilter) << mem_sink_parent->type_string();

  OperatorIR* filter_parent = filter->parents()[0];
  ASSERT_EQ(filter_parent->type(), IRNodeType::kUnion) << mem_sink_parent->type_string();

  // Check to see that the union's parents are memory_sources.
  std::vector<types::TabletID> tablet_values;
  UnionIR* union_op = static_cast<UnionIR*>(filter_parent);

  EXPECT_TRUE(union_op->HasColumnMappings());
  EXPECT_TRUE(union_op->IsRelationInit());

  for (auto* union_op_parent : union_op->parents()) {
    ASSERT_EQ(union_op_parent->type(), IRNodeType::kMemorySource) << union_op_parent->type_string();
    auto mem_source = static_cast<MemorySourceIR*>(union_op_parent);
    ASSERT_TRUE(mem_source->HasTablet()) << union_op_parent->type_string();
    tablet_values.push_back(mem_source->tablet_value());
    EXPECT_TRUE(mem_source->IsRelationInit());
    EXPECT_FALSE(mem_source->IsTimeSet());
  }

  EXPECT_THAT(tablet_values, ElementsAreArray(in_tablet_values));
}

TEST_F(MemorySourceTabletRuleTest, tablet_source_no_match) {
  Relation relation({types::UINT128, types::INT64, types::STRING}, {"upid", "cpu0", "name"});
  auto mem_src = MakeMemSource("table", relation);
  std::vector<types::TabletID> tablet_values = {"tablet1", "tablet2"};

  TabletSourceGroupIR* tablet_source_group = MakeTabletSourceGroup(mem_src, tablet_values, "upid");
  // should not match any of the tablets above.

  FuncIR* filter_expr = MakeEqualsFunc(MakeColumn("upid", 0), MakeString("tablet3"));

  filter_expr->SetOutputDataType(types::BOOLEAN);
  FilterIR* filter = MakeFilter(tablet_source_group, filter_expr);
  MakeMemSink(filter, "out");

  MemorySourceTabletRule rule;
  auto result = rule.Execute(graph.get());
  ASSERT_NOT_OK(result);
  EXPECT_THAT(result.status(),
              HasCompilerError("Number of matching tablets must be greater than 0."));
}

using TabletizerTest = TabletSourceConversionRuleTest;
TEST_F(TabletizerTest, combined_tests) {
  distributedpb::CarnotInfo carnot_info;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kCarnotInfo, &carnot_info));

  std::vector<types::TabletID> expected_tablet_values;
  ASSERT_EQ(carnot_info.table_info()[0].tablets_size(), 2);
  expected_tablet_values.push_back(carnot_info.table_info()[0].tablets(0));
  expected_tablet_values.push_back(carnot_info.table_info()[0].tablets(1));

  // This table has tablet keys so it should be transformed as appropriate.
  auto mem_src = MakeMemSource("cpu_table", relation0_);
  auto mem_sink = MakeMemSink(mem_src, "out");

  int64_t mem_src_id = mem_src->id();

  auto result = Tabletizer::Execute(carnot_info, graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  // Check to see that the group is no longer available
  EXPECT_FALSE(graph->HasNode(mem_src_id));

  // Check to see that mem_sink1 has a new parent that is a union.
  ASSERT_EQ(mem_sink->parents().size(), 1UL);
  OperatorIR* mem_sink_parent = mem_sink->parents()[0];
  ASSERT_EQ(mem_sink_parent->type(), IRNodeType::kUnion) << mem_sink_parent->type_string();

  // Check to see that the union's parents are memory_sourcesj
  std::vector<types::TabletID> actual_tablet_values;
  UnionIR* union_op = static_cast<UnionIR*>(mem_sink_parent);

  EXPECT_TRUE(union_op->HasColumnMappings());
  EXPECT_TRUE(union_op->IsRelationInit());

  for (auto* union_op_parent : union_op->parents()) {
    ASSERT_EQ(union_op_parent->type(), IRNodeType::kMemorySource) << union_op_parent->type_string();
    auto mem_source = static_cast<MemorySourceIR*>(union_op_parent);
    ASSERT_TRUE(mem_source->HasTablet()) << union_op_parent->type_string();
    actual_tablet_values.push_back(mem_source->tablet_value());
    EXPECT_TRUE(mem_source->IsRelationInit());
    EXPECT_FALSE(mem_source->IsTimeSet());
  }

  EXPECT_THAT(actual_tablet_values, ElementsAreArray(expected_tablet_values));

  for (int64_t i : graph->dag().TopologicalSort()) {
    IRNode* node = graph->Get(i);
    EXPECT_FALSE(Match(node, TabletSourceGroup()))
        << "Tablet source group should not exist at this point.";
  }
}

TEST_F(TabletizerTest, no_table_info_for_memory_source) {
  distributedpb::CarnotInfo carnot_info;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kCarnotInfo, &carnot_info));

  // This table doesn't have tablet keys so the rule should not do anything.
  auto mem_src = MakeMemSource("other_table", relation0_);
  auto mem_sink = MakeMemSink(mem_src, "out");

  int64_t mem_src_id = mem_src->id();

  auto result = Tabletizer::Execute(carnot_info, graph.get());
  EXPECT_OK(result);
  EXPECT_FALSE(result.ConsumeValueOrDie());

  // Check to see that the group is no longer available
  EXPECT_TRUE(graph->HasNode(mem_src_id));

  // Check to see that mem_sink1 has a new parent that is a union.
  ASSERT_EQ(mem_sink->parents().size(), 1UL);
  OperatorIR* mem_sink_parent = mem_sink->parents()[0];
  ASSERT_EQ(mem_sink_parent->type(), IRNodeType::kMemorySource);
  auto mem_source = static_cast<MemorySourceIR*>(mem_sink_parent);
  EXPECT_FALSE(mem_source->HasTablet());
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
