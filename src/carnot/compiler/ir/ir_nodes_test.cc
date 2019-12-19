#include <queue>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <pypa/ast/ast.hh>

#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/ir/pattern_match.h"
#include "src/carnot/compiler/metadata_handler.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/common/testing/protobuf.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace compiler {
using ::pl::testing::proto::EqualsProto;
using table_store::schema::Relation;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::UnorderedElementsAre;

TEST(IRTypes, types_enum_test) {
  // Quick test to make sure the enums test is inline with the type strings.
  EXPECT_EQ(static_cast<int64_t>(IRNodeType::number_of_types),
            sizeof(kIRNodeStrings) / sizeof(*kIRNodeStrings));
}

/**
 * Creates IR Graph that is the following query compiled
 *
 * `pl.DataFrame(table="tableName", select=["testCol"], start_time"-2m")`
 */

TEST(IRTest, CreateSource) {
  auto ast = MakeTestAstPtr();
  auto ig = std::make_shared<IR>();
  auto src = ig->CreateNode<MemorySourceIR>(ast, "table_str", std::vector<std::string>{"testCol"})
                 .ValueOrDie();
  auto start_rng_str = ig->CreateNode<IntIR>(ast, 0).ValueOrDie();
  auto stop_rng_str = ig->CreateNode<IntIR>(ast, 10).ValueOrDie();

  EXPECT_OK(src->SetTimeExpressions(start_rng_str, stop_rng_str));

  EXPECT_EQ(src->start_time_expr(), start_rng_str);
  EXPECT_EQ(src->end_time_expr(), stop_rng_str);

  EXPECT_EQ(src->table_name(), "table_str");
  EXPECT_THAT(src->column_names(), ElementsAre("testCol"));
}

TEST(IRTest, CreateSourceSharedNodes) {
  auto ast = MakeTestAstPtr();
  auto ig = std::make_shared<IR>();
  auto src = ig->CreateNode<MemorySourceIR>(ast, "table_str", std::vector<std::string>{"testCol"})
                 .ValueOrDie();
  auto time_expr = ig->CreateNode<IntIR>(ast, 10).ValueOrDie();

  EXPECT_OK(src->SetTimeExpressions(time_expr, time_expr));
  EXPECT_NE(src->start_time_expr()->id(), src->end_time_expr()->id());
  CompareClone(src->start_time_expr(), src->end_time_expr(),
               "MemorySource start/end time expression");
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

TEST(IRTest, CollectionSharedNodes) {
  auto ast = MakeTestAstPtr();
  auto ig = std::make_shared<IR>();
  auto expr = ig->CreateNode<IntIR>(ast, 10).ValueOrDie();
  std::vector<ExpressionIR*> children{expr, expr};

  auto list = ig->CreateNode<ListIR>(ast, children).ValueOrDie();
  ASSERT_EQ(2, list->children().size());
  EXPECT_NE(list->children()[0]->id(), list->children()[1]->id());
  CompareClone(list->children()[0], list->children()[1], "List expression");

  auto tuple = ig->CreateNode<TupleIR>(ast, children).ValueOrDie();
  ASSERT_EQ(2, tuple->children().size());
  EXPECT_NE(tuple->children()[0]->id(), tuple->children()[1]->id());
  CompareClone(tuple->children()[0], tuple->children()[1], "Tuple expression");
}

const char* kExpectedMemSrcPb = R"(
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

TEST(ToProto, memory_source_ir) {
  auto ast = MakeTestAstPtr();
  auto graph = std::make_shared<IR>();

  auto mem_src =
      graph->CreateNode<MemorySourceIR>(ast, "test_table", std::vector<std::string>{}).ValueOrDie();
  EXPECT_OK(mem_src->SetRelation(
      Relation({types::DataType::INT64, types::DataType::FLOAT64}, {"cpu0", "cpu1"})));

  mem_src->SetColumnIndexMap({0, 1});
  mem_src->SetTimeValuesNS(10, 20);

  planpb::Operator pb;
  EXPECT_OK(mem_src->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(kExpectedMemSrcPb));
}

const char* kExpectedMemSrcWithTabletPb = R"(
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

    tablet: $0
  }
)";

TEST(ToProto, memory_source_ir_with_tablet) {
  auto ast = MakeTestAstPtr();
  auto graph = std::make_shared<IR>();

  auto mem_src =
      graph->CreateNode<MemorySourceIR>(ast, "test_table", std::vector<std::string>{"cpu0", "cpu1"})
          .ConsumeValueOrDie();

  EXPECT_OK(mem_src->SetRelation(
      Relation({types::DataType::INT64, types::DataType::FLOAT64}, {"cpu0", "cpu1"})));

  mem_src->SetColumnIndexMap({0, 2});
  mem_src->SetTimeValuesNS(10, 20);

  types::TabletID tablet_value = "abcd";

  mem_src->SetTabletValue(tablet_value);

  planpb::Operator pb;
  EXPECT_OK(mem_src->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(absl::Substitute(kExpectedMemSrcWithTabletPb,
                                               absl::Substitute("\"$0\"", tablet_value))));
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
  auto ast = MakeTestAstPtr();
  auto graph = std::make_shared<IR>();

  auto mem_source =
      graph->CreateNode<MemorySourceIR>(ast, "source", std::vector<std::string>{}).ValueOrDie();
  auto mem_sink = graph
                      ->CreateNode<MemorySinkIR>(ast, mem_source, "output_table",
                                                 std::vector<std::string>{"output1", "output2"})
                      .ValueOrDie();

  auto rel = table_store::schema::Relation(
      std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64}),
      std::vector<std::string>({"output1", "output2"}));
  EXPECT_OK(mem_sink->SetRelation(rel));

  planpb::Operator pb;
  EXPECT_OK(mem_sink->ToProto(&pb));

  planpb::Operator expected_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kExpectedMemSinkPb, &expected_pb));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(expected_pb, pb));
}

const char* kExpectedMapPb = R"(
  op_type: MAP_OPERATOR
  map_op {
    column_names: "col_name"
    expressions {
      func {
        id: 1
        name: "pl.add"
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
      }
    }
  }
)";

TEST(ToProto, map_ir) {
  auto ast = MakeTestAstPtr();
  auto graph = std::make_shared<IR>();
  auto constant = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto col = graph->CreateNode<ColumnIR>(ast, "col4", /*parent_op_idx*/ 0).ValueOrDie();
  col->ResolveColumn(4, types::INT64);
  auto func = graph
                  ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::add, "+", "add"},
                                       std::vector<ExpressionIR*>({constant, col}))
                  .ValueOrDie();
  func->set_func_id(1);

  auto mem_src =
      graph
          ->CreateNode<MemorySourceIR>(
              ast, "table_name", std::vector<std::string>{"col0", "col1", "col2", "col3", "col4"})
          .ValueOrDie();
  auto map = graph
                 ->CreateNode<MapIR>(ast, mem_src, ColExpressionVector{{"col_name", func}},
                                     /* keep_input_columns */ false)
                 .ValueOrDie();

  planpb::Operator pb;
  EXPECT_OK(map->ToProto(&pb));

  planpb::Operator expected_pb;
  ASSERT_TRUE(google::protobuf::TextFormat::MergeFromString(kExpectedMapPb, &expected_pb));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(expected_pb, pb));
}

const char* kExpectedAggPb = R"(
  op_type: AGGREGATE_OPERATOR
  agg_op {
    windowed: false
    values {
      name: "pl.mean"
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
          index: 4
        }
      }
    }
    groups {
      node: 0
      index: 1
    }
    group_names: "group1"
    value_names: "mean"
  }
)";

TEST(ToProto, agg_ir) {
  auto ast = MakeTestAstPtr();
  auto graph = std::make_shared<IR>();
  auto mem_src =
      graph->CreateNode<MemorySourceIR>(ast, "source", std::vector<std::string>{}).ValueOrDie();
  auto constant = graph->CreateNode<IntIR>(ast, 10).ValueOrDie();
  auto col = graph->CreateNode<ColumnIR>(ast, "column", /*parent_op_idx*/ 0).ValueOrDie();
  col->ResolveColumn(4, types::INT64);

  auto agg_func = graph
                      ->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", "mean"},
                                           std::vector<ExpressionIR*>{constant, col})
                      .ValueOrDie();

  auto group1 = graph->CreateNode<ColumnIR>(ast, "group1", /*parent_op_idx*/ 0).ValueOrDie();
  group1->ResolveColumn(1, types::INT64);

  auto agg = graph
                 ->CreateNode<BlockingAggIR>(ast, mem_src, std::vector<ColumnIR*>{group1},
                                             ColExpressionVector{{"mean", agg_func}})
                 .ValueOrDie();

  planpb::Operator pb;
  ASSERT_OK(agg->ToProto(&pb));

  planpb::Operator expected_pb;
  EXPECT_THAT(pb, EqualsProto(kExpectedAggPb));
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

TEST_F(MetadataTests, metadata_resolver) {
  MetadataResolverIR* metadata_resolver =
      graph->CreateNode<MetadataResolverIR>(ast, MakeMemSource()).ValueOrDie();
  MetadataProperty* md_property = md_handler->GetProperty("pod_name").ValueOrDie();
  EXPECT_FALSE(metadata_resolver->HasMetadataColumn("pod_name"));
  EXPECT_OK(metadata_resolver->AddMetadata(md_property));
  EXPECT_TRUE(metadata_resolver->HasMetadataColumn("pod_name"));
  EXPECT_EQ(metadata_resolver->metadata_columns().size(), 1);
  EXPECT_EQ(metadata_resolver->metadata_columns().find("pod_name")->second, md_property);
}

TEST_F(MetadataTests, metadata_ir) {
  MetadataResolverIR* metadata_resolver =
      graph->CreateNode<MetadataResolverIR>(ast, MakeMemSource()).ValueOrDie();
  MetadataIR* metadata_ir =
      graph->CreateNode<MetadataIR>(ast, "pod_name", /*parent_op_idx*/ 0).ValueOrDie();
  EXPECT_TRUE(metadata_ir->IsColumn());
  EXPECT_FALSE(metadata_ir->HasMetadataResolver());
  EXPECT_EQ(metadata_ir->name(), "pod_name");
  auto property = std::make_unique<NameMetadataProperty>(
      MetadataType::POD_NAME, std::vector<MetadataType>({MetadataType::POD_ID}));
  EXPECT_OK(metadata_ir->ResolveMetadataColumn(metadata_resolver, property.get()));
  EXPECT_TRUE(metadata_ir->HasMetadataResolver());
}

// Swapping a parent should make sure that all columns are passed over correclt.
TEST_F(OperatorTests, swap_parent) {
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

TEST_F(OperatorTests, grpc_ops) {
  int64_t grpc_id = 123;
  std::string source_grpc_address = "1111";
  std::string sink_physical_id = "agent-xyz";

  MemorySourceIR* mem_src = MakeMemSource();
  GRPCSinkIR* grpc_sink = MakeGRPCSink(mem_src, grpc_id);

  std::shared_ptr<IR> new_graph = std::make_shared<IR>();

  // swaps the graph being built and returns the old_graph
  std::shared_ptr<IR> old_graph = SwapGraphBeingBuilt(new_graph);

  GRPCSourceGroupIR* grpc_src_group = MakeGRPCSourceGroup(grpc_id, MakeRelation());
  MakeMemSink(grpc_src_group, "out");

  grpc_src_group->SetGRPCAddress(source_grpc_address);
  EXPECT_EQ(grpc_sink->destination_id(), grpc_id);
  EXPECT_OK(grpc_src_group->AddGRPCSink(grpc_sink));
  EXPECT_EQ(grpc_src_group->source_id(), grpc_id);
}

using CloneTests = OperatorTests;
TEST_F(CloneTests, simple_clone) {
  auto mem_source = MakeMemSource();
  ColumnIR* col1 = MakeColumn("test1", 0);
  ColumnIR* col2 = MakeColumn("test2", 0);
  ColumnIR* col3 = MakeColumn("test3", 0);
  FuncIR* add_func = MakeAddFunc(col3, MakeInt(3));
  MapIR* map = MakeMap(mem_source, {{{"out1", col1}, {"out2", col2}, {"out3", add_func}}, {}});
  MakeMemSink(map, "out");

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
  auto mem_source = MakeMemSource();
  IntIR* intnode = MakeInt(105);
  FuncIR* add_func = MakeAddFunc(intnode, MakeInt(3));
  FuncIR* add_func2 = MakeAddFunc(intnode, add_func);
  MapIR* map1 = MakeMap(mem_source, {{{"int", intnode}, {"add", add_func}}});
  MapIR* map2 = MakeMap(map1, {{{"add1", add_func}, {"add2", add_func2}}});
  MakeMemSink(map2, "out");

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
  auto mem_source = MakeMemSource();
  auto filter =
      MakeFilter(mem_source, MakeEqualsFunc(MakeMetadataIR("service", 0),
                                            MakeMetadataLiteral(MakeString("pl/test_service"))));
  auto limit = MakeLimit(filter, 10);

  auto agg = MakeBlockingAgg(limit, {MakeMetadataIR("service", 0)},
                             {{{"mean", MakeMeanFunc(MakeColumn("equals_column", 0))}}, {}});
  auto map = MakeMap(agg, {{{"mean_deux", MakeAddFunc(MakeColumn("mean", 0), MakeInt(3))},
                            {"mean", MakeColumn("mean", 0)}},
                           {}});
  MakeMemSink(map, "sup");
  auto out = graph->Clone();
  EXPECT_OK(out.status());
  std::unique_ptr<IR> cloned_ir = out.ConsumeValueOrDie();

  ASSERT_EQ(graph->dag().TopologicalSort(), cloned_ir->dag().TopologicalSort());

  // Make sure that all of the columns are now part of the new graph.
  for (int64_t i : cloned_ir->dag().TopologicalSort()) {
    CompareClone(cloned_ir->Get(i), graph->Get(i), absl::Substitute("For index $0", i));
  }
}

TEST_F(CloneTests, clone_grpc_source_group_and_sink) {
  // Build graph 1.
  auto grpc_source = MakeGRPCSourceGroup(123, MakeRelation());
  MakeMemSink(grpc_source, "sup");
  grpc_source->SetGRPCAddress("1111");

  auto graph2 = std::make_shared<IR>();
  auto graph1 = SwapGraphBeingBuilt(graph2);

  // Build graph 2.
  auto mem_source = MakeMemSource();
  GRPCSinkIR* grpc_sink = MakeGRPCSink(mem_source, 123);
  grpc_sink->SetDestinationAddress("1111");

  EXPECT_OK(grpc_source->AddGRPCSink(grpc_sink));

  auto out = graph1->Clone();
  EXPECT_OK(out.status());
  std::unique_ptr<IR> cloned_ir1 = out.ConsumeValueOrDie();

  ASSERT_EQ(graph1->dag().TopologicalSort(), cloned_ir1->dag().TopologicalSort());

  // Make sure that all of the columns are now part of the new graph.
  for (int64_t i : cloned_ir1->dag().TopologicalSort()) {
    CompareClone(cloned_ir1->Get(i), graph1->Get(i), absl::Substitute("For index $0", i));
  }

  out = graph2->Clone();
  EXPECT_OK(out.status());
  std::unique_ptr<IR> cloned_ir2 = out.ConsumeValueOrDie();

  ASSERT_EQ(graph2->dag().TopologicalSort(), cloned_ir2->dag().TopologicalSort());

  // Make sure that all of the columns are now part of the new graph.
  for (int64_t i : cloned_ir2->dag().TopologicalSort()) {
    CompareClone(cloned_ir2->Get(i), graph2->Get(i), absl::Substitute("For index $0", i));
  }
}

TEST_F(CloneTests, grpc_source) {
  auto grpc_source = MakeGRPCSource(MakeRelation());
  MakeMemSink(grpc_source, "sup");
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
  auto mem_src1 = MakeMemSource(relation0);

  Relation relation1({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64, types::DataType::INT64},
                     {"right_only", "col1", "col2", "col3", "col4"});
  auto mem_src2 = MakeMemSource(relation1);

  auto join_op =
      MakeJoin({mem_src1, mem_src2}, "inner", relation0, relation1,
               std::vector<std::string>{"col1", "col3"}, std::vector<std::string>{"col2", "col4"});

  auto out = graph->Clone();

  EXPECT_OK(out.status());
  std::unique_ptr<IR> cloned_ir = out.ConsumeValueOrDie();
  ASSERT_EQ(graph->dag().TopologicalSort(), cloned_ir->dag().TopologicalSort());

  graph->Get(join_op->id());
  IRNode* maybe_join_clone = cloned_ir->Get(join_op->id());
  ASSERT_EQ(maybe_join_clone->type(), IRNodeType::kJoin);
  JoinIR* join_clone = static_cast<JoinIR*>(maybe_join_clone);

  CompareClone(join_clone, join_op, "");
}

TEST_F(CloneTests, copy_into_existing_dag) {
  auto src = MakeMemSource();
  IntIR* intnode = MakeInt(105);
  FuncIR* add_func = MakeAddFunc(intnode, MakeInt(3));
  FuncIR* add_func2 = MakeAddFunc(intnode, add_func);
  MapIR* map = MakeMap(src, {{{"int", intnode}, {"add1", add_func}, {"add2", add_func2}}});

  auto source_out = graph->CopyNode(src);
  EXPECT_OK(source_out.status());
  auto map_out = graph->CopyNode(map);
  EXPECT_OK(source_out.status());
  auto cmap = static_cast<OperatorIR*>(map_out.ConsumeValueOrDie());
  auto csrc = static_cast<OperatorIR*>(source_out.ConsumeValueOrDie());
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

class ToProtoTests : public OperatorTests {};
const char* kExpectedGRPCSourcePb = R"proto(
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
  auto grpc_src = MakeGRPCSource(MakeRelation());
  MakeMemSink(grpc_src, "sink");

  planpb::Operator pb;
  ASSERT_OK(grpc_src->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(kExpectedGRPCSourcePb));
}

const char* kExpectedGRPCSinkPb = R"proto(
  op_type: GRPC_SINK_OPERATOR
  grpc_sink_op {
    address: "$0"
    destination_id: $1
  }
)proto";

TEST_F(ToProtoTests, grpc_sink_ir) {
  int64_t destination_id = 123;
  std::string grpc_address = "1111";
  std::string physical_id = "agent-aa";
  auto mem_src = MakeMemSource();
  auto grpc_sink = MakeGRPCSink(mem_src, destination_id);
  grpc_sink->SetDestinationAddress(grpc_address);

  planpb::Operator pb;
  ASSERT_OK(grpc_sink->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(absl::Substitute(kExpectedGRPCSinkPb, grpc_address, destination_id)));
}

const char* kIRProto = R"proto(
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
      }
    }
  }
}
)proto";
TEST_F(ToProtoTests, ir) {
  auto mem_src = MakeMemSource(MakeRelation());
  auto mem_sink = MakeMemSink(mem_src, "out");
  EXPECT_OK(mem_sink->SetRelation(MakeRelation()));

  planpb::Plan pb = graph->ToProto().ConsumeValueOrDie();

  EXPECT_THAT(pb, EqualsProto(kIRProto));
}

const char* kExpectedUnionOpPb = R"proto(
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

  auto mem_src1 = MakeMemSource(relation);
  auto mem_src2 = MakeMemSource(relation2);
  auto union_op = MakeUnion({mem_src1, mem_src2});
  EXPECT_OK(union_op->SetRelation(relation));

  EXPECT_OK(union_op->SetRelationFromParents());
  planpb::Operator pb;
  EXPECT_OK(union_op->ToProto(&pb));
  EXPECT_THAT(pb, EqualsProto(kExpectedUnionOpPb));
}

const char* kExpectedUnionOpTimePb = R"proto(
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
  auto mem_src1 = MakeMemSource(relation);
  std::reverse(std::begin(column_names), std::end(column_names));
  std::reverse(std::begin(column_types), std::end(column_types));
  relation = Relation(column_types, column_names);
  auto mem_src2 = MakeMemSource(relation);
  auto union_op = MakeUnion({mem_src1, mem_src2});
  EXPECT_OK(union_op->SetRelation(mem_src1->relation()));

  EXPECT_OK(union_op->SetRelationFromParents());
  planpb::Operator pb;
  EXPECT_OK(union_op->ToProto(&pb));
  EXPECT_THAT(pb, EqualsProto(kExpectedUnionOpTimePb));
}

const char* kExpectedInnerJoinOpPb = R"proto(
op_type: JOIN_OPERATOR
join_op {
  type: INNER
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
    parent_index: 1
    column_index: 4
  }
  output_columns {
    parent_index: 0
    column_index: 1
  }
  output_columns {
    parent_index: 1
    column_index: 0
  }
  column_names: "left_only"
  column_names: "col4"
  column_names: "col1"
  column_names: "right_only"
}
)proto";

TEST_F(ToProtoTests, inner_join) {
  Relation relation0({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64},
                     {"left_only", "col1", "col2", "col3"});
  auto mem_src1 = MakeMemSource(relation0);

  Relation relation1({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64, types::DataType::INT64},
                     {"right_only", "col1", "col2", "col3", "col4"});
  auto mem_src2 = MakeMemSource(relation1);

  auto join_op =
      MakeJoin({mem_src1, mem_src2}, "inner", relation0, relation1,
               std::vector<std::string>{"col1", "col3"}, std::vector<std::string>{"col2", "col4"});

  std::vector<std::string> col_names{"left_only", "col4", "col1", "right_only"};
  std::vector<ColumnIR*> cols{MakeColumn("left_only", 0, relation0),
                              MakeColumn("col4", 1, relation1), MakeColumn("col1", 0, relation0),
                              MakeColumn("right_only", 1, relation1)};
  EXPECT_OK(join_op->SetOutputColumns(col_names, cols));

  planpb::Operator pb;
  EXPECT_OK(join_op->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(kExpectedInnerJoinOpPb));
}

const char* kExpectedLeftJoinOpPb = R"proto(
op_type: JOIN_OPERATOR
join_op {
  type: LEFT_OUTER
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
    parent_index: 1
    column_index: 4
  }
  output_columns {
    parent_index: 0
    column_index: 1
  }
  output_columns {
    parent_index: 1
    column_index: 0
  }
  column_names: "left_only"
  column_names: "col4"
  column_names: "col1"
  column_names: "right_only"
}
)proto";

TEST_F(ToProtoTests, left_join) {
  Relation relation0({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64},
                     {"left_only", "col1", "col2", "col3"});
  auto mem_src1 = MakeMemSource(relation0);

  Relation relation1({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64, types::DataType::INT64},
                     {"right_only", "col1", "col2", "col3", "col4"});
  auto mem_src2 = MakeMemSource(relation1);

  auto join_op =
      MakeJoin({mem_src1, mem_src2}, "left", relation0, relation1,
               std::vector<std::string>{"col1", "col3"}, std::vector<std::string>{"col2", "col4"});
  std::vector<std::string> col_names{"left_only", "col4", "col1", "right_only"};
  std::vector<ColumnIR*> cols{MakeColumn("left_only", 0, relation0),
                              MakeColumn("col4", 1, relation1), MakeColumn("col1", 0, relation0),
                              MakeColumn("right_only", 1, relation1)};
  EXPECT_OK(join_op->SetOutputColumns(col_names, cols));

  planpb::Operator pb;
  EXPECT_OK(join_op->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(kExpectedLeftJoinOpPb));
}

const char* kExpectedOuterJoinOpPb = R"proto(
op_type: JOIN_OPERATOR
join_op {
  type: FULL_OUTER
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
    parent_index: 1
    column_index: 4
  }
  output_columns {
    parent_index: 0
    column_index: 1
  }
  output_columns {
    parent_index: 1
    column_index: 0
  }
  column_names: "left_only"
  column_names: "col4"
  column_names: "col1"
  column_names: "right_only"
}
)proto";

TEST_F(ToProtoTests, full_outer) {
  Relation relation0({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64},
                     {"left_only", "col1", "col2", "col3"});
  auto mem_src1 = MakeMemSource(relation0);

  Relation relation1({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64, types::DataType::INT64},
                     {"right_only", "col1", "col2", "col3", "col4"});
  auto mem_src2 = MakeMemSource(relation1);

  auto join_op =
      MakeJoin({mem_src1, mem_src2}, "outer", relation0, relation1,
               std::vector<std::string>{"col1", "col3"}, std::vector<std::string>{"col2", "col4"});
  std::vector<std::string> col_names{"left_only", "col4", "col1", "right_only"};
  std::vector<ColumnIR*> cols{MakeColumn("left_only", 0, relation0),
                              MakeColumn("col4", 1, relation1), MakeColumn("col1", 0, relation0),
                              MakeColumn("right_only", 1, relation1)};
  EXPECT_OK(join_op->SetOutputColumns(col_names, cols));

  planpb::Operator pb;
  EXPECT_OK(join_op->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(kExpectedOuterJoinOpPb));
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

TEST_F(OperatorTests, op_children) {
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

using IRPruneTests = OperatorTests;
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

TEST_F(OperatorTests, tablet_source_group) {
  auto mem_source = MakeMemSource("table", MakeRelation());
  std::vector<types::TabletID> tablet_values = {"tablet1", "tablet2"};
  auto tablet_source =
      graph->CreateNode<TabletSourceGroupIR>(ast, mem_source, tablet_values, "cpu0")
          .ConsumeValueOrDie();

  EXPECT_THAT(tablet_source->tablets(), ElementsAreArray(tablet_values));
  EXPECT_EQ(tablet_source->ReplacedMemorySource(), mem_source);
}

TEST_F(OperatorTests, GroupByNode) {
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

TEST_F(OperatorTests, join_node) {
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

TEST_F(OperatorTests, join_node_mismatched_columns) {
  auto mem_source1 = MakeMemSource();
  auto mem_source2 = MakeMemSource();

  EXPECT_NOT_OK(graph->CreateNode<JoinIR>(
      ast, std::vector<OperatorIR*>{mem_source1, mem_source2}, "inner",
      std::vector<ColumnIR*>{MakeColumn("col1", 0), MakeColumn("col2", 0)},
      std::vector<ColumnIR*>{MakeColumn("col1", 1)}, std::vector<std::string>{}));
}

TEST_F(OperatorTests, join_duplicate_parents) {
  Relation relation0({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64},
                     {"left_only", "col1", "col2", "col3"});
  auto mem_src = MakeMemSource(relation0);

  auto join_node =
      MakeJoin({mem_src, mem_src}, "inner", relation0, relation0, {"col1"}, {"col1"}, {"_x", "_y"});

  auto map_node_status = FindNodeType(graph, IRNodeType::kMap);
  EXPECT_OK(map_node_status);
  auto map_node = static_cast<MapIR*>(map_node_status.ConsumeValueOrDie());
  EXPECT_THAT(join_node->parents(), ElementsAre(mem_src, map_node));
}

TEST_F(OperatorTests, union_duplicate_parents) {
  Relation relation0({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64},
                     {"left_only", "col1", "col2", "col3"});
  auto mem_src1 = MakeMemSource(relation0);

  Relation relation1({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64, types::DataType::INT64},
                     {"right_only", "col1", "col2", "col3", "col4"});
  auto mem_src2 = MakeMemSource(relation1);
  auto union_op = MakeUnion({mem_src2, mem_src1, mem_src1, mem_src1});

  std::vector<OperatorIR*> maps;
  for (size_t i = 0; i < 2; ++i) {
    auto map_node_status = FindNodeType(graph, IRNodeType::kMap, i);
    EXPECT_OK(map_node_status);
    auto map_node = static_cast<MapIR*>(map_node_status.ConsumeValueOrDie());
    maps.push_back(map_node);
  }

  EXPECT_THAT(union_op->parents(), ElementsAre(mem_src2, mem_src1, maps[0], maps[1]));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
