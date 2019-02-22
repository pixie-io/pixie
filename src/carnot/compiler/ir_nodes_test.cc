#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/ir_test_utils.h"
#include "src/carnot/plan/relation.h"

namespace pl {
namespace carnot {
namespace compiler {
/**
 * Creates IR Graph that is the following query compiled
 *
 * `From(table="tableName", select=["testCol"]).Range("-2m")`
 */

TEST(IRTest, check_connection) {
  auto ig = std::make_shared<IR>();
  auto src = ig->MakeNode<MemorySourceIR>().ValueOrDie();
  auto range = ig->MakeNode<RangeIR>().ValueOrDie();
  auto rng_str = ig->MakeNode<StringIR>().ValueOrDie();
  auto table_str = ig->MakeNode<StringIR>().ValueOrDie();
  auto select_col = ig->MakeNode<StringIR>().ValueOrDie();
  auto select_list = ig->MakeNode<ListIR>().ValueOrDie();
  EXPECT_TRUE(rng_str->Init("-2m").ok());
  EXPECT_TRUE(table_str->Init("tableName").ok());
  EXPECT_TRUE(select_col->Init("testCol").ok());
  EXPECT_TRUE(select_list->AddListItem(select_col).ok());
  EXPECT_TRUE(src->Init(table_str, select_list).ok());
  EXPECT_TRUE(range->Init(src, rng_str).ok());
  EXPECT_EQ(range->parent(), src);
  EXPECT_EQ(range->time_repr(), rng_str);
  EXPECT_EQ(src->table_node(), table_str);
  EXPECT_EQ(src->select(), select_list);
  EXPECT_EQ(select_list->children()[0], select_col);
  EXPECT_EQ(select_col->str(), "testCol");
  VerifyGraphConnections(ig.get());
}

TEST(IRWalker, basic_tests) {
  // Construct example IR Graph.
  auto graph = std::make_shared<IR>();

  // Create nodes.
  auto src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  auto select_list = graph->MakeNode<ListIR>().ValueOrDie();
  auto map = graph->MakeNode<MapIR>().ValueOrDie();
  auto agg = graph->MakeNode<AggIR>().ValueOrDie();
  auto sink = graph->MakeNode<MemorySinkIR>().ValueOrDie();

  // Add dependencies.
  EXPECT_OK(graph->AddEdge(src, select_list));
  EXPECT_OK(graph->AddEdge(src, map));
  EXPECT_OK(graph->AddEdge(map, agg));
  EXPECT_OK(graph->AddEdge(agg, sink));

  std::vector<int64_t> call_order;
  auto s = IRWalker()
               .OnMemorySink([&](auto& mem_sink) {
                 call_order.push_back(mem_sink.id());
                 return Status::OK();
               })
               .OnMemorySource([&](auto& mem_src) {
                 call_order.push_back(mem_src.id());
                 return Status::OK();
               })
               .OnMap([&](auto& map) {
                 call_order.push_back(map.id());
                 return Status::OK();
               })
               .OnAgg([&](auto& agg) {
                 call_order.push_back(agg.id());
                 return Status::OK();
               })
               .Walk(*graph);
  EXPECT_OK(s);
  EXPECT_EQ(std::vector<int64_t>({0, 2, 3, 4}), call_order);
}

const char* kExpectedMemSrcPb = R"(
  op_type: MEMORY_SOURCE_OPERATOR
  mem_source_op {
    name: "test_table"
    column_idxs: 0
    column_idxs: 2
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

TEST(ToProto, memory_source_ir) {
  auto graph = std::make_shared<IR>();

  auto mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  auto select_list = graph->MakeNode<ListIR>().ValueOrDie();
  auto table_node = graph->MakeNode<StringIR>().ValueOrDie();
  EXPECT_OK(table_node->Init("test_table"));
  EXPECT_OK(mem_src->Init(table_node, select_list));

  auto col_1 = graph->MakeNode<ColumnIR>().ValueOrDie();
  EXPECT_OK(col_1->Init("cpu0"));
  col_1->SetColumnIdx(0);
  col_1->SetColumnType(types::DataType::INT64);

  auto col_2 = graph->MakeNode<ColumnIR>().ValueOrDie();
  EXPECT_OK(col_2->Init("cpu1"));
  col_2->SetColumnIdx(2);
  col_2->SetColumnType(types::DataType::FLOAT64);

  mem_src->SetColumns(std::vector<ColumnIR*>({col_1, col_2}));
  mem_src->SetTime(10, 20);

  carnotpb::Operator pb;
  EXPECT_OK(mem_src->ToProto(&pb));

  carnotpb::Operator expected_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kExpectedMemSrcPb, &expected_pb));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(expected_pb, pb));
}

const char* kExpectedMemSinkPb = R"(
  op_type: MEMORY_SINK_OPERATOR
  mem_sink_op {
    name: "output_table"
    column_names: "output1"
    column_names: "output2"
    column_types: INT64
    column_types: FLOAT64
  }
)";

TEST(ToProto, memory_sink_ir) {
  auto graph = std::make_shared<IR>();

  auto mem_sink = graph->MakeNode<MemorySinkIR>().ValueOrDie();
  auto mem_source = graph->MakeNode<MemorySourceIR>().ValueOrDie();

  auto rel = plan::Relation(
      std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64}),
      std::vector<std::string>({"output1", "output2"}));
  EXPECT_OK(mem_sink->SetRelation(rel));
  EXPECT_OK(mem_sink->Init(mem_source, "output_table"));

  carnotpb::Operator pb;
  EXPECT_OK(mem_sink->ToProto(&pb));

  carnotpb::Operator expected_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kExpectedMemSinkPb, &expected_pb));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(expected_pb, pb));
}

const char* kExpectedMapPb = R"(
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
            node: 0
            index: 4
          }
        }
      }
    }
  }
)";

TEST(ToProto, map_ir) {
  auto graph = std::make_shared<IR>();
  auto mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  auto map = graph->MakeNode<MapIR>().ValueOrDie();
  auto constant = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(constant->Init(10));
  auto col = graph->MakeNode<ColumnIR>().ValueOrDie();
  col->SetColumnIdx(4);
  auto func = graph->MakeNode<FuncIR>().ValueOrDie();
  EXPECT_OK(func->Init("add", std::vector<IRNode*>({constant, col})));
  EXPECT_OK(map->Init(mem_src, func));
  auto expr_map = std::unordered_map<std::string, IRNode*>();
  expr_map.emplace("col_name", func);
  map->SetColExprMap(expr_map);

  carnotpb::Operator pb;
  EXPECT_OK(map->ToProto(&pb));

  carnotpb::Operator expected_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kExpectedMapPb, &expected_pb));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(expected_pb, pb));
}

const char* kExpectedAggPb = R"(
  op_type: BLOCKING_AGGREGATE_OPERATOR
  blocking_agg_op {
    values {
      name: "mean"
      args {
        constant {
          data_type: INT64
          int64_value: 10
        }
      }
      args {
        column {
          node: 0
          index: 4
        }
      }
    }
    groups {
      node: 0
      index: 1
    }
    group_names: "group1"
    value_names: "value1"
  }
)";

TEST(ToProto, agg_ir) {
  auto graph = std::make_shared<IR>();
  auto mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  auto agg = graph->MakeNode<AggIR>().ValueOrDie();
  auto constant = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(constant->Init(10));
  auto col = graph->MakeNode<ColumnIR>().ValueOrDie();
  col->SetColumnIdx(4);

  auto func = graph->MakeNode<FuncIR>().ValueOrDie();
  auto by_func = graph->MakeNode<FuncIR>().ValueOrDie();
  EXPECT_OK(func->Init("mean", std::vector<IRNode*>({constant, col})));
  EXPECT_OK(agg->Init(mem_src, by_func, func));
  auto expr_map = std::unordered_map<std::string, IRNode*>();
  expr_map.emplace("value1", func);
  agg->SetAggValMap(expr_map);

  auto group1 = graph->MakeNode<ColumnIR>().ValueOrDie();
  EXPECT_OK(group1->Init("group1"));
  group1->SetColumnIdx(1);
  agg->SetGroups(std::vector<ColumnIR*>({group1}));

  carnotpb::Operator pb;
  EXPECT_OK(agg->ToProto(&pb));

  carnotpb::Operator expected_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kExpectedAggPb, &expected_pb));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(expected_pb, pb));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
