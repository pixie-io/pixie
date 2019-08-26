#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/tablet_rules.h"
#include "src/carnot/compiler/test_utils.h"
namespace pl {
namespace carnot {
namespace compiler {
namespace physical {
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

using TabletSourceConversionRuleTest = OperatorTests;

const char* kCarnotInfo = R"proto(
query_broker_address: "carnot"
has_data_store: true
processes_data: true
accepts_remote_sources: false
table_info {
  table: "cpu_table"
  relation{
    columns {
      column_name: "time_"
      column_type: TIME64NS
    }
    columns {
      column_name: "upid"
      column_type: UINT128
    }
    columns {
      column_name: "cycles"
      column_type: INT64
    }
  }
  tabletization_key: "upid"
  tablets: "1"
  tablets: "2"
}
table_info {
  table: "network"
  relation {
    columns {
      column_name: "time_"
      column_type: TIME64NS
    }
    columns {
      column_name: "read_bytes"
      column_type: INT64
    }
  }
}
)proto";

TEST_F(TabletSourceConversionRuleTest, simple_test) {
  compilerpb::CarnotInfo carnot_info;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kCarnotInfo, &carnot_info));

  Relation relation0;
  EXPECT_OK(relation0.FromProto(&(carnot_info.table_info()[0].relation())));

  Relation relation1;
  EXPECT_OK(relation1.FromProto(&(carnot_info.table_info()[1].relation())));

  // This table has tablet keys so it should be transformed as appropriate.
  auto mem_src0 = MakeMemSource("cpu_table", relation0);
  auto mem_sink0 = MakeMemSink(mem_src0, "out");

  // This table does not have tablet keys so ti should not be transformed.
  auto mem_src1 = MakeMemSource("network", relation0);
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

using MemorySourceTabletRuleTest = OperatorTests;
TEST_F(MemorySourceTabletRuleTest, tablet_source_group_unions) {
  Relation relation({types::UINT128, types::INT64, types::STRING}, {"upid", "cpu0", "name"});
  auto mem_src = MakeMemSource("table", relation);
  TabletSourceGroupIR* tablet_source_group =
      MakeTabletSourceGroup(mem_src, {"tablet1", "tablet2"}, "upid");
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
  std::vector<TabletKeyType> tablet_values;
  UnionIR* union_op = static_cast<UnionIR*>(mem_sink_parent);

  EXPECT_TRUE(union_op->HasColumnMappings());
  EXPECT_TRUE(union_op->IsRelationInit());

  for (auto* union_op_parent : union_op->parents()) {
    ASSERT_EQ(union_op_parent->type(), IRNodeType::kMemorySource) << union_op_parent->type_string();
    auto mem_source = static_cast<MemorySourceIR*>(union_op_parent);
    ASSERT_TRUE(mem_source->HasTablet()) << union_op_parent->type_string();
    tablet_values.push_back(mem_source->tablet_value());
    EXPECT_TRUE(mem_source->IsRelationInit());
  }

  EXPECT_THAT(tablet_values, ElementsAre("tablet1", "tablet2"));
}

TEST_F(MemorySourceTabletRuleTest, tablet_source_group_no_union) {
  Relation relation({types::UINT128, types::INT64, types::STRING}, {"upid", "cpu0", "name"});
  auto mem_src = MakeMemSource("table", relation);
  TabletSourceGroupIR* tablet_source_group = MakeTabletSourceGroup(mem_src, {"tablet1"}, "upid");
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
  std::string tablet_value = new_mem_src->tablet_value();
  EXPECT_TRUE(new_mem_src->IsRelationInit());
  EXPECT_EQ(tablet_value, "tablet1");
}

// TODO(philkuz) enable when we have support for filters.
TEST_F(MemorySourceTabletRuleTest, DISABLED_tablet_source_group_union_tabletization_key_filter) {
  Relation relation({types::UINT128, types::INT64, types::STRING}, {"upid", "cpu0", "name"});
  auto mem_src = MakeMemSource("table", relation);
  TabletSourceGroupIR* tablet_source_group =
      MakeTabletSourceGroup(mem_src, {"tablet1", "tablet2"}, "upid");
  FilterIR* filter =
      MakeFilter(tablet_source_group, MakeEqualsFunc(MakeColumn("upid", 0), MakeString("tablet2")));
  MemorySinkIR* mem_sink = MakeMemSink(filter, "out");

  int64_t tablet_source_group_id = tablet_source_group->id();
  int64_t filter_id = filter->id();

  MemorySourceTabletRule rule;
  auto result = rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  // Check to see that the group is no longer available
  EXPECT_FALSE(graph->HasNode(tablet_source_group_id));
  EXPECT_FALSE(graph->HasNode(filter_id));

  // Check to see that mem_sink1 has a new parent that is a union.
  ASSERT_EQ(mem_sink->parents().size(), 1UL);
  OperatorIR* mem_sink_parent = mem_sink->parents()[0];
  ASSERT_EQ(mem_sink_parent->type(), IRNodeType::kMemorySource) << mem_sink_parent->type_string();

  // Check to see that the union's parents are memory_sourcesj
  MemorySourceIR* new_mem_src = static_cast<MemorySourceIR*>(mem_sink_parent);
  ASSERT_TRUE(new_mem_src->HasTablet());
  std::string tablet_value = new_mem_src->tablet_value();
  EXPECT_TRUE(new_mem_src->IsRelationInit());
  EXPECT_EQ(tablet_value, "tablet");
}

// TODO(philkuz) enable when we have support for filters.
TEST_F(MemorySourceTabletRuleTest,
       DISABLED_tablet_source_group_union_tabletization_key_filter_and) {
  Relation relation({types::UINT128, types::INT64, types::STRING}, {"upid", "cpu0", "name"});
  auto mem_src = MakeMemSource("table", relation);
  TabletSourceGroupIR* tablet_source_group =
      MakeTabletSourceGroup(mem_src, {"tablet1", "tablet2", "tablet3"}, "upid");
  FilterIR* filter =
      MakeFilter(tablet_source_group,
                 MakeAndFunc(MakeEqualsFunc(MakeColumn("upid", 0), MakeString("tablet2")),
                             MakeEqualsFunc(MakeColumn("upid", 0), MakeString("tablet3"))));
  MemorySinkIR* mem_sink = MakeMemSink(filter, "out");

  int64_t tablet_source_group_id = tablet_source_group->id();
  int64_t filter_id = filter->id();

  MemorySourceTabletRule rule;
  auto result = rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  // Check to see that the group is no longer available
  EXPECT_FALSE(graph->HasNode(tablet_source_group_id));
  EXPECT_FALSE(graph->HasNode(filter_id));

  // Check to see that mem_sink1 has a new parent that is a union.
  // Check to see that mem_sink1 has a new parent that is a union.
  ASSERT_EQ(mem_sink->parents().size(), 1UL);
  OperatorIR* mem_sink_parent = mem_sink->parents()[0];
  ASSERT_EQ(mem_sink_parent->type(), IRNodeType::kUnion) << mem_sink_parent->type_string();

  // Check to see that the union's parents are memory_sources.
  std::vector<TabletKeyType> tablet_values;
  UnionIR* union_op = static_cast<UnionIR*>(mem_sink_parent);

  EXPECT_TRUE(union_op->HasColumnMappings());
  EXPECT_TRUE(union_op->IsRelationInit());

  for (auto* union_op_parent : union_op->parents()) {
    ASSERT_EQ(union_op_parent->type(), IRNodeType::kMemorySource) << union_op_parent->type_string();
    auto mem_source = static_cast<MemorySourceIR*>(union_op_parent);
    ASSERT_TRUE(mem_source->HasTablet()) << union_op_parent->type_string();
    tablet_values.push_back(mem_source->tablet_value());
    EXPECT_TRUE(mem_source->IsRelationInit());
  }

  EXPECT_THAT(tablet_values, ElementsAre("tablet2", "tablet3"));
}

// TODO(philkuz) enable when we have support for filters.
TEST_F(MemorySourceTabletRuleTest, DISABLED_tablet_source_group_filter_does_nothing) {
  Relation relation({types::UINT128, types::INT64, types::STRING}, {"upid", "cpu0", "name"});
  auto mem_src = MakeMemSource("table", relation);
  TabletSourceGroupIR* tablet_source_group =
      MakeTabletSourceGroup(mem_src, {"tablet1", "tablet2", "tablet3"}, "upid");
  FilterIR* filter =
      MakeFilter(tablet_source_group, MakeEqualsFunc(MakeColumn("name", 0), MakeString("blah")));
  MemorySinkIR* mem_sink = MakeMemSink(filter, "out");

  int64_t tablet_source_group_id = tablet_source_group->id();
  int64_t filter_id = filter->id();

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
  std::vector<TabletKeyType> tablet_values;
  UnionIR* union_op = static_cast<UnionIR*>(filter_parent);

  EXPECT_TRUE(union_op->HasColumnMappings());
  EXPECT_TRUE(union_op->IsRelationInit());

  for (auto* union_op_parent : union_op->parents()) {
    ASSERT_EQ(union_op_parent->type(), IRNodeType::kMemorySource) << union_op_parent->type_string();
    auto mem_source = static_cast<MemorySourceIR*>(union_op_parent);
    ASSERT_TRUE(mem_source->HasTablet()) << union_op_parent->type_string();
    tablet_values.push_back(mem_source->tablet_value());
    EXPECT_TRUE(mem_source->IsRelationInit());
  }

  EXPECT_THAT(tablet_values, ElementsAre("tablet1", "tablet2", "tablet3"));
}

using TabletRulesIntegrationTest = OperatorTests;
TEST_F(TabletRulesIntegrationTest, combined_tests) {
  compilerpb::CarnotInfo carnot_info;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kCarnotInfo, &carnot_info));

  Relation relation0;
  EXPECT_OK(relation0.FromProto(&(carnot_info.table_info()[0].relation())));

  std::vector<std::string> expected_tablet_values;
  ASSERT_EQ(carnot_info.table_info()[0].tablets_size(), 2);
  expected_tablet_values.push_back(carnot_info.table_info()[0].tablets(0));
  expected_tablet_values.push_back(carnot_info.table_info()[0].tablets(1));

  // This table has tablet keys so it should be transformed as appropriate.
  auto mem_src = MakeMemSource("cpu_table", relation0);
  auto mem_sink = MakeMemSink(mem_src, "out");

  int64_t mem_src_id = mem_src->id();

  TabletSourceConversionRule tabletization_rule(carnot_info);
  auto result = tabletization_rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  MemorySourceTabletRule rule;
  result = rule.Execute(graph.get());
  EXPECT_OK(result);
  EXPECT_TRUE(result.ConsumeValueOrDie());

  // Check to see that the group is no longer available
  EXPECT_FALSE(graph->HasNode(mem_src_id));

  // Check to see that mem_sink1 has a new parent that is a union.
  ASSERT_EQ(mem_sink->parents().size(), 1UL);
  OperatorIR* mem_sink_parent = mem_sink->parents()[0];
  ASSERT_EQ(mem_sink_parent->type(), IRNodeType::kUnion) << mem_sink_parent->type_string();

  // Check to see that the union's parents are memory_sourcesj
  std::vector<TabletKeyType> actual_tablet_values;
  UnionIR* union_op = static_cast<UnionIR*>(mem_sink_parent);

  EXPECT_TRUE(union_op->HasColumnMappings());
  EXPECT_TRUE(union_op->IsRelationInit());

  for (auto* union_op_parent : union_op->parents()) {
    ASSERT_EQ(union_op_parent->type(), IRNodeType::kMemorySource) << union_op_parent->type_string();
    auto mem_source = static_cast<MemorySourceIR*>(union_op_parent);
    ASSERT_TRUE(mem_source->HasTablet()) << union_op_parent->type_string();
    actual_tablet_values.push_back(mem_source->tablet_value());
    EXPECT_TRUE(mem_source->IsRelationInit());
  }

  EXPECT_THAT(actual_tablet_values, ElementsAreArray(expected_tablet_values));

  for (int64_t i : graph->dag().TopologicalSort()) {
    IRNode* node = graph->Get(i);
    EXPECT_FALSE(Match(node, TabletSourceGroup()))
        << "Tablet source group should not exist at this point.";
  }
}

}  // namespace physical
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
