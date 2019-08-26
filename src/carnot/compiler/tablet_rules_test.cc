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

}  // namespace physical
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
