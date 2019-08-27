#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <pypa/ast/ast.hh>

#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/ir_test_utils.h"
#include "src/carnot/compiler/metadata_handler.h"
#include "src/carnot/compiler/pattern_match.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/common/testing/protobuf.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace compiler {
using ::pl::testing::proto::EqualsProto;
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
 * `From(table="tableName", select=["testCol"]).Range("-2m")`
 */

TEST(IRTest, check_connection) {
  auto ast = MakeTestAstPtr();
  auto ig = std::make_shared<IR>();
  auto src = ig->MakeNode<MemorySourceIR>().ValueOrDie();
  auto range = ig->MakeNode<RangeIR>().ValueOrDie();
  auto start_rng_str = ig->MakeNode<IntIR>().ValueOrDie();
  auto stop_rng_str = ig->MakeNode<IntIR>().ValueOrDie();
  auto table_str_node = ig->MakeNode<StringIR>().ValueOrDie();
  auto select_col = ig->MakeNode<StringIR>().ValueOrDie();
  auto select_list = ig->MakeNode<ListIR>().ValueOrDie();
  EXPECT_OK(start_rng_str->Init(0, ast));
  EXPECT_OK(stop_rng_str->Init(10, ast));
  std::string table_str = "tableName";
  EXPECT_OK(table_str_node->Init(table_str, ast));
  EXPECT_OK(select_col->Init("testCol", ast));
  EXPECT_OK(select_list->Init(ast, {select_col}));
  ArgMap memsrc_argmap({{"table", table_str_node}, {"select", select_list}});
  EXPECT_OK(src->Init(nullptr, memsrc_argmap, ast));
  EXPECT_OK(range->Init(src, start_rng_str, stop_rng_str, ast));
  EXPECT_EQ(range->parents()[0], src);
  EXPECT_EQ(range->start_repr(), start_rng_str);
  EXPECT_EQ(range->stop_repr(), stop_rng_str);
  EXPECT_EQ(src->table_name(), table_str);
  EXPECT_THAT(src->column_names(), ElementsAre("testCol"));
  EXPECT_EQ(select_list->children()[0], select_col);
  EXPECT_EQ(select_col->str(), "testCol");
  VerifyGraphConnections(ig.get());
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
  auto ast = MakeTestAstPtr();
  auto graph = std::make_shared<IR>();

  auto mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  auto select_list = graph->MakeNode<ListIR>().ValueOrDie();
  auto table_node = graph->MakeNode<StringIR>().ValueOrDie();
  EXPECT_OK(table_node->Init("test_table", ast));
  ArgMap memsrc_argmap({{"table", table_node}, {"select", select_list}});
  EXPECT_OK(mem_src->Init(nullptr, memsrc_argmap, ast));

  EXPECT_OK(mem_src->SetRelation(
      Relation({types::DataType::INT64, types::DataType::FLOAT64}, {"cpu0", "cpu1"})));

  mem_src->SetColumnIndexMap({0, 2});
  mem_src->SetTime(10, 20);

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

    tablet: "abcd"
  }
)";

TEST(ToProto, memory_source_ir_with_tablet) {
  auto ast = MakeTestAstPtr();
  auto graph = std::make_shared<IR>();

  auto mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  auto select_list = graph->MakeNode<ListIR>().ValueOrDie();
  auto table_node = graph->MakeNode<StringIR>().ValueOrDie();
  EXPECT_OK(table_node->Init("test_table", ast));
  ArgMap memsrc_argmap({{"table", table_node}, {"select", select_list}});
  EXPECT_OK(mem_src->Init(nullptr, memsrc_argmap, ast));

  EXPECT_OK(mem_src->SetRelation(
      Relation({types::DataType::INT64, types::DataType::FLOAT64}, {"cpu0", "cpu1"})));

  mem_src->SetColumnIndexMap({0, 2});
  mem_src->SetTime(10, 20);

  mem_src->SetTablet("abcd");

  planpb::Operator pb;
  EXPECT_OK(mem_src->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(kExpectedMemSrcWithTabletPb));
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

  auto mem_sink = graph->MakeNode<MemorySinkIR>().ValueOrDie();
  auto mem_source = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  auto name_ir = graph->MakeNode<StringIR>().ValueOrDie();

  auto rel = table_store::schema::Relation(
      std::vector<types::DataType>({types::DataType::INT64, types::DataType::FLOAT64}),
      std::vector<std::string>({"output1", "output2"}));
  EXPECT_OK(mem_sink->SetRelation(rel));
  EXPECT_OK(name_ir->Init("output_table", ast));
  ArgMap amap({{"name", name_ir}});
  EXPECT_OK(mem_sink->Init(mem_source, amap, ast));

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
            node: 0
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
  auto mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  auto map = graph->MakeNode<MapIR>().ValueOrDie();
  auto constant = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(constant->Init(10, ast));
  auto col = graph->MakeNode<ColumnIR>().ValueOrDie();
  EXPECT_OK(col->Init("col_name", /*parent_op_idx*/ 0, ast));
  col->ResolveColumn(4, types::INT64);
  auto func = graph->MakeNode<FuncIR>().ValueOrDie();
  auto lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
  EXPECT_OK(func->Init({FuncIR::Opcode::add, "+", "add"}, ASTWalker::kRunTimeFuncPrefix,
                       std::vector<ExpressionIR*>({constant, col}), false /* compile_time */, ast));
  func->set_func_id(1);
  EXPECT_OK(lambda->Init({"col_name"}, {{"col_name", func}}, ast));
  ArgMap amap({{"fn", lambda}});
  EXPECT_OK(map->Init(mem_src, amap, ast));

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
  auto mem_src = graph->MakeNode<MemorySourceIR>().ValueOrDie();
  auto agg = graph->MakeNode<BlockingAggIR>().ValueOrDie();
  auto constant = graph->MakeNode<IntIR>().ValueOrDie();
  EXPECT_OK(constant->Init(10, ast));
  auto col = graph->MakeNode<ColumnIR>().ValueOrDie();
  EXPECT_OK(col->Init("column", /*parent_op_idx*/ 0, ast));
  col->ResolveColumn(4, types::INT64);

  auto agg_func_lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
  auto agg_func = graph->MakeNode<FuncIR>().ValueOrDie();
  EXPECT_OK(agg_func->Init({FuncIR::Opcode::non_op, "", "mean"}, ASTWalker::kRunTimeFuncPrefix,
                           std::vector<ExpressionIR*>({constant, col}), false /* compile_time */,
                           ast));
  EXPECT_OK(agg_func_lambda->Init({"meaned_column"}, {{"mean", agg_func}}, ast));

  auto by_func_lambda = graph->MakeNode<LambdaIR>().ValueOrDie();
  auto group1 = graph->MakeNode<ColumnIR>().ValueOrDie();
  EXPECT_OK(group1->Init("group1", /*parent_op_idx*/ 0, ast));
  group1->ResolveColumn(1, types::INT64);
  EXPECT_OK(by_func_lambda->Init({"group1"}, group1, ast));
  ArgMap amap({{"by", by_func_lambda}, {"fn", agg_func_lambda}});

  ASSERT_OK(agg->Init(mem_src, amap, ast));
  ColExpressionVector exprs;
  exprs.push_back(ColumnExpression({"value1", agg_func}));

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
  MemorySourceIR* MakeMemSource() { return graph->MakeNode<MemorySourceIR>().ValueOrDie(); }
  pypa::AstPtr ast;
  std::shared_ptr<IR> graph;
  std::unique_ptr<MetadataHandler> md_handler;
};

TEST_F(MetadataTests, metadata_resolver) {
  MetadataResolverIR* metadata_resolver = graph->MakeNode<MetadataResolverIR>().ValueOrDie();
  EXPECT_OK(metadata_resolver->Init(MakeMemSource(), {{}}, ast));
  MetadataProperty* md_property = md_handler->GetProperty("pod_name").ValueOrDie();
  EXPECT_FALSE(metadata_resolver->HasMetadataColumn("pod_name"));
  EXPECT_OK(metadata_resolver->AddMetadata(md_property));
  EXPECT_TRUE(metadata_resolver->HasMetadataColumn("pod_name"));
  EXPECT_EQ(metadata_resolver->metadata_columns().size(), 1);
  EXPECT_EQ(metadata_resolver->metadata_columns().find("pod_name")->second, md_property);
}

TEST_F(MetadataTests, metadata_ir) {
  MetadataResolverIR* metadata_resolver = graph->MakeNode<MetadataResolverIR>().ValueOrDie();
  MetadataIR* metadata_ir = graph->MakeNode<MetadataIR>().ValueOrDie();
  EXPECT_OK(metadata_ir->Init("pod_name", /*parent_op_idx*/ 0, ast));
  EXPECT_TRUE(metadata_ir->IsColumn());
  EXPECT_FALSE(metadata_ir->HasMetadataResolver());
  EXPECT_EQ(metadata_ir->name(), "pod_name");
  EXPECT_OK(metadata_resolver->Init(MakeMemSource(), {{}}, ast));
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
  MapIR* child_map = MakeMap(mem_source, {{"out11", col1}, {"out2", col2}, {"out3", add_func}});
  EXPECT_EQ(col1->ReferenceID().ConsumeValueOrDie(), mem_source->id());
  EXPECT_EQ(col2->ReferenceID().ConsumeValueOrDie(), mem_source->id());
  EXPECT_EQ(col3->ReferenceID().ConsumeValueOrDie(), mem_source->id());

  // Insert a map as if we are copying from the parent. These columns are distinact from col1-3.
  MapIR* parent_map = MakeMap(mem_source, {{"test1", MakeColumn("test1", /*parent_op_idx*/ 0)},
                                           {"test2", MakeColumn("test2", /*parent_op_idx*/ 0)},
                                           {"test3", MakeColumn("test3", /*parent_op_idx*/ 0)}});

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
  std::string expected_physical_dest_id = absl::Substitute("$0:$1", sink_physical_id, grpc_id);

  MemorySourceIR* mem_src = MakeMemSource();
  GRPCSinkIR* grpc_sink = MakeGRPCSink(mem_src, grpc_id);

  std::shared_ptr<IR> new_graph = std::make_shared<IR>();

  // swaps the graph being built and returns the old_graph
  std::shared_ptr<IR> old_graph = SwapGraphBeingBuilt(new_graph);

  GRPCSourceGroupIR* grpc_src_group = MakeGRPCSourceGroup(grpc_id, MakeRelation());
  MakeMemSink(grpc_src_group, "out");

  grpc_src_group->SetGRPCAddress(source_grpc_address);
  grpc_sink->SetDistributedID(sink_physical_id);
  EXPECT_EQ(grpc_sink->DistributedDestinationID(), expected_physical_dest_id);
  EXPECT_OK(grpc_src_group->AddGRPCSink(grpc_sink));
  EXPECT_EQ(grpc_src_group->remote_string_ids(),
            std::vector<std::string>({expected_physical_dest_id}));
}
class CloneTests : public OperatorTests {
 protected:
  void CompareClonedColumn(ColumnIR* new_ir, ColumnIR* old_ir, const std::string& failure_string) {
    if (new_ir->graph_ptr() != old_ir->graph_ptr()) {
      EXPECT_NE(new_ir->ContainingOperator().ConsumeValueOrDie()->graph_ptr(),
                old_ir->ContainingOperator().ConsumeValueOrDie()->graph_ptr())
          << absl::Substitute(
                 "'$1' and '$2' should have container ops that are in different graphs. $0.",
                 failure_string, new_ir->DebugString(), old_ir->DebugString());
    }
    EXPECT_EQ(new_ir->ReferencedOperator().ConsumeValueOrDie()->id(),
              old_ir->ReferencedOperator().ConsumeValueOrDie()->id())
        << failure_string;
    EXPECT_EQ(new_ir->col_name(), old_ir->col_name()) << failure_string;
  }
  void CompareClonedMap(MapIR* new_ir, MapIR* old_ir, const std::string& failure_string) {
    std::vector<ColumnExpression> new_col_exprs = new_ir->col_exprs();
    std::vector<ColumnExpression> old_col_exprs = old_ir->col_exprs();
    ASSERT_EQ(new_col_exprs.size(), old_col_exprs.size()) << failure_string;
    for (size_t i = 0; i < new_col_exprs.size(); ++i) {
      ColumnExpression new_expr = new_col_exprs[i];
      ColumnExpression old_expr = old_col_exprs[i];
      EXPECT_EQ(new_expr.name, old_expr.name) << failure_string;
      EXPECT_EQ(new_expr.node->type_string(), old_expr.node->type_string()) << failure_string;
      EXPECT_EQ(new_expr.node->id(), old_expr.node->id()) << failure_string;
    }
  }

  void CompareClonedBlockingAgg(BlockingAggIR* new_ir, BlockingAggIR* old_ir,
                                const std::string& failure_string) {
    std::vector<ColumnExpression> new_col_exprs = new_ir->aggregate_expressions();
    std::vector<ColumnExpression> old_col_exprs = old_ir->aggregate_expressions();
    ASSERT_EQ(new_col_exprs.size(), old_col_exprs.size()) << failure_string;
    for (size_t i = 0; i < new_col_exprs.size(); ++i) {
      ColumnExpression new_expr = new_col_exprs[i];
      ColumnExpression old_expr = old_col_exprs[i];
      EXPECT_EQ(new_expr.name, old_expr.name) << failure_string;
      EXPECT_EQ(new_expr.node->type_string(), old_expr.node->type_string()) << failure_string;
      EXPECT_EQ(new_expr.node->id(), old_expr.node->id()) << failure_string;
    }

    std::vector<ColumnIR*> new_groups = new_ir->groups();
    std::vector<ColumnIR*> old_groups = old_ir->groups();
    ASSERT_EQ(new_groups.size(), old_groups.size()) << failure_string;
    for (size_t i = 0; i < new_groups.size(); ++i) {
      CompareClonedColumn(new_groups[i], old_groups[i], failure_string);
    }
  }
  void CompareClonedMetadata(MetadataIR* new_ir, MetadataIR* old_ir,
                             const std::string& err_string) {
    CompareClonedColumn(new_ir, old_ir, err_string);
    EXPECT_EQ(new_ir->property(), old_ir->property())
        << absl::Substitute("Expected Metadata properties to be the same. Got $1 vs $2. $0.",
                            err_string, new_ir->property()->name(), old_ir->property()->name());
    EXPECT_EQ(new_ir->name(), old_ir->name())
        << absl::Substitute("Expected Metadata names to be the same. Got $1 vs $2. $0.", err_string,
                            new_ir->name(), old_ir->name());
  }
  void CompareClonedMetadataLiteral(MetadataLiteralIR* new_ir, MetadataLiteralIR* old_ir,
                                    const std::string& err_string) {
    EXPECT_EQ(new_ir->literal_type(), old_ir->literal_type()) << err_string;
    EXPECT_EQ(new_ir->literal()->id(), old_ir->literal()->id()) << err_string;
  }
  void CompareClonedMemorySource(MemorySourceIR* new_ir, MemorySourceIR* old_ir,
                                 const std::string& err_string) {
    EXPECT_EQ(new_ir->table_name(), old_ir->table_name()) << err_string;
    EXPECT_EQ(new_ir->IsTimeSet(), old_ir->IsTimeSet()) << err_string;
    EXPECT_EQ(new_ir->time_start_ns(), old_ir->time_start_ns()) << err_string;
    EXPECT_EQ(new_ir->time_stop_ns(), old_ir->time_stop_ns()) << err_string;
    EXPECT_EQ(new_ir->column_names(), old_ir->column_names()) << err_string;
    EXPECT_EQ(new_ir->column_index_map_set(), old_ir->column_index_map_set()) << err_string;
  }
  void CompareClonedMemorySink(MemorySinkIR* new_ir, MemorySinkIR* old_ir,
                               const std::string& err_string) {
    EXPECT_EQ(new_ir->name(), old_ir->name()) << err_string;
    EXPECT_EQ(new_ir->name_set(), old_ir->name_set()) << err_string;
  }
  void CompareClonedFilter(FilterIR* new_ir, FilterIR* old_ir, const std::string& err_string) {
    CompareClonedExpression(new_ir->filter_expr(), old_ir->filter_expr(), err_string);
  }
  void CompareClonedLimit(LimitIR* new_ir, LimitIR* old_ir, const std::string& err_string) {
    EXPECT_EQ(new_ir->limit_value(), old_ir->limit_value()) << err_string;
    EXPECT_EQ(new_ir->limit_value_set(), old_ir->limit_value_set()) << err_string;
  }
  void CompareClonedFunc(FuncIR* new_ir, FuncIR* old_ir, const std::string& err_string) {
    EXPECT_EQ(new_ir->func_name(), old_ir->func_name()) << err_string;
    EXPECT_EQ(new_ir->op().op_code, old_ir->op().op_code) << err_string;
    EXPECT_EQ(new_ir->op().python_op, old_ir->op().python_op) << err_string;
    EXPECT_EQ(new_ir->op().carnot_op_name, old_ir->op().carnot_op_name) << err_string;
    EXPECT_EQ(new_ir->func_id(), old_ir->func_id()) << err_string;
    EXPECT_EQ(new_ir->is_compile_time(), old_ir->is_compile_time()) << err_string;
    EXPECT_EQ(new_ir->IsDataTypeEvaluated(), old_ir->IsDataTypeEvaluated()) << err_string;
    EXPECT_EQ(new_ir->EvaluatedDataType(), old_ir->EvaluatedDataType()) << err_string;

    std::vector<ExpressionIR*> new_args = new_ir->args();
    std::vector<ExpressionIR*> old_args = old_ir->args();
    ASSERT_EQ(new_args.size(), old_args.size()) << err_string;
    for (size_t i = 0; i < new_args.size(); ++i) {
      CompareClonedExpression(new_args[i], old_args[i], err_string);
    }
  }

  void CompareClonedGRPCSourceGroup(GRPCSourceGroupIR* new_ir, GRPCSourceGroupIR* old_ir,
                                    const std::string& err_string) {
    EXPECT_EQ(new_ir->source_id(), old_ir->source_id()) << err_string;
    EXPECT_EQ(new_ir->remote_string_ids(), old_ir->remote_string_ids()) << err_string;
    EXPECT_EQ(new_ir->grpc_address(), old_ir->grpc_address()) << err_string;
    EXPECT_EQ(new_ir->GRPCAddressSet(), old_ir->GRPCAddressSet()) << err_string;
  }

  void CompareClonedGRPCSink(GRPCSinkIR* new_ir, GRPCSinkIR* old_ir,
                             const std::string& err_string) {
    EXPECT_EQ(new_ir->destination_id(), old_ir->destination_id()) << err_string;
    EXPECT_EQ(new_ir->DistributedDestinationID(), old_ir->DistributedDestinationID()) << err_string;
    EXPECT_EQ(new_ir->DistributedIDSet(), old_ir->DistributedIDSet()) << err_string;
    EXPECT_EQ(new_ir->destination_address(), old_ir->destination_address()) << err_string;
    EXPECT_EQ(new_ir->DestinationAddressSet(), old_ir->DestinationAddressSet()) << err_string;
  }

  void CompareClonedGRPCSource(GRPCSourceIR* new_ir, GRPCSourceIR* old_ir,
                               const std::string& err_string) {
    EXPECT_EQ(new_ir->remote_source_id(), old_ir->remote_source_id()) << err_string;
  }

  void CompareClonedJoin(JoinIR* new_ir, JoinIR* old_ir, const std::string& err_string) {
    ASSERT_EQ(new_ir->join_type(), old_ir->join_type());
    EXPECT_THAT(new_ir->column_names(), ElementsAreArray(old_ir->column_names())) << err_string;
    auto output_columns_new = new_ir->output_columns();
    auto output_columns_old = old_ir->output_columns();
    ASSERT_EQ(output_columns_new.size(), output_columns_old.size()) << err_string;

    for (size_t i = 0; i < output_columns_new.size(); ++i) {
      CompareClonedExpression(output_columns_new[i], output_columns_old[i],
                              absl::Substitute("$0; in Join operator.", err_string));
    }

    auto old_equality_conditions = old_ir->equality_conditions();
    auto new_equality_conditions = new_ir->equality_conditions();
    ASSERT_EQ(old_equality_conditions.size(), new_equality_conditions.size()) << err_string;

    for (size_t i = 0; i < old_equality_conditions.size(); ++i) {
      EXPECT_EQ(old_equality_conditions[i].left_column_idx,
                new_equality_conditions[i].left_column_idx);
      EXPECT_EQ(old_equality_conditions[i].right_column_idx,
                new_equality_conditions[i].right_column_idx);
    }

    auto old_column_names = old_ir->column_names();
    auto new_column_names = new_ir->column_names();
    ASSERT_EQ(old_column_names.size(), new_column_names.size()) << err_string;

    for (size_t i = 0; i < old_column_names.size(); ++i) {
      EXPECT_EQ(old_column_names[i], new_column_names[i]);
    }
  }

  void CompareClonedExpression(ExpressionIR* new_ir, ExpressionIR* old_ir,
                               const std::string& err_string) {
    ASSERT_NE(new_ir, nullptr);
    ASSERT_NE(old_ir, nullptr);
    if (Match(new_ir, ColumnNode())) {
      CompareClonedColumn(static_cast<ColumnIR*>(new_ir), static_cast<ColumnIR*>(old_ir),
                          err_string);
    } else if (Match(new_ir, Func())) {
      CompareClonedFunc(static_cast<FuncIR*>(new_ir), static_cast<FuncIR*>(old_ir), err_string);

    } else if (Match(new_ir, MetadataLiteral())) {
      CompareClonedMetadataLiteral(static_cast<MetadataLiteralIR*>(new_ir),
                                   static_cast<MetadataLiteralIR*>(old_ir), err_string);
    } else if (Match(new_ir, Metadata())) {
      CompareClonedMetadata(static_cast<MetadataIR*>(new_ir), static_cast<MetadataIR*>(old_ir),
                            err_string);
    }
  }

  void CompareClonedOperator(OperatorIR* new_ir, OperatorIR* old_ir,
                             const std::string& err_string) {
    std::string new_err_string =
        absl::Substitute("$0. In $1 Operator.", err_string, new_ir->type_string());
    if (Match(new_ir, MemorySource())) {
      CompareClonedMemorySource(static_cast<MemorySourceIR*>(new_ir),
                                static_cast<MemorySourceIR*>(old_ir), new_err_string);
    } else if (Match(new_ir, MemorySink())) {
      CompareClonedMemorySink(static_cast<MemorySinkIR*>(new_ir),
                              static_cast<MemorySinkIR*>(old_ir), new_err_string);
    } else if (Match(new_ir, Filter())) {
      CompareClonedFilter(static_cast<FilterIR*>(new_ir), static_cast<FilterIR*>(old_ir),
                          new_err_string);
    } else if (Match(new_ir, Limit())) {
      CompareClonedLimit(static_cast<LimitIR*>(new_ir), static_cast<LimitIR*>(old_ir),
                         new_err_string);
    } else if (Match(new_ir, Map())) {
      CompareClonedMap(static_cast<MapIR*>(new_ir), static_cast<MapIR*>(old_ir), new_err_string);
    } else if (Match(new_ir, BlockingAgg())) {
      CompareClonedBlockingAgg(static_cast<BlockingAggIR*>(new_ir),
                               static_cast<BlockingAggIR*>(old_ir), new_err_string);

    } else if (Match(new_ir, GRPCSink())) {
      CompareClonedGRPCSink(static_cast<GRPCSinkIR*>(new_ir), static_cast<GRPCSinkIR*>(old_ir),
                            new_err_string);
    } else if (Match(new_ir, GRPCSourceGroup())) {
      CompareClonedGRPCSourceGroup(static_cast<GRPCSourceGroupIR*>(new_ir),
                                   static_cast<GRPCSourceGroupIR*>(old_ir), new_err_string);
    } else if (Match(new_ir, GRPCSource())) {
      CompareClonedGRPCSource(static_cast<GRPCSourceIR*>(new_ir),
                              static_cast<GRPCSourceIR*>(old_ir), new_err_string);
    } else {
      EXPECT_TRUE(false) << absl::Substitute("Couldn't check operator cloning for $0. $1",
                                             new_ir->type_string(), err_string);
    }

    // Check relation status.
    EXPECT_EQ(new_ir->IsRelationInit(), old_ir->IsRelationInit());
    EXPECT_EQ(new_ir->relation().col_names(), old_ir->relation().col_names());
    EXPECT_EQ(new_ir->relation().col_types(), old_ir->relation().col_types());

    // Check parents.
    ASSERT_EQ(new_ir->parents().size(), old_ir->parents().size());
    for (size_t parent_idx = 0; parent_idx < new_ir->parents().size(); ++parent_idx) {
      EXPECT_EQ(new_ir->parents()[parent_idx]->DebugString(),
                old_ir->parents()[parent_idx]->DebugString());
    }
  }
  void CompareClonedNodes(IRNode* new_ir, IRNode* old_ir, const std::string& err_string) {
    EXPECT_NE(old_ir, new_ir) << err_string;
    ASSERT_EQ(old_ir->type_string(), new_ir->type_string()) << err_string;
    if (Match(new_ir, Expression())) {
      CompareClonedExpression(static_cast<ExpressionIR*>(new_ir),
                              static_cast<ExpressionIR*>(old_ir), err_string);
    } else if (Match(new_ir, Operator())) {
      CompareClonedOperator(static_cast<OperatorIR*>(new_ir), static_cast<OperatorIR*>(old_ir),
                            err_string);
    }
  }
};

TEST_F(CloneTests, simple_clone) {
  auto mem_source = MakeMemSource();
  ColumnIR* col1 = MakeColumn("test1", 0);
  ColumnIR* col2 = MakeColumn("test2", 0);
  ColumnIR* col3 = MakeColumn("test3", 0);
  FuncIR* add_func = MakeAddFunc(col3, MakeInt(3));
  MapIR* map = MakeMap(mem_source, {{"out1", col1}, {"out2", col2}, {"out3", add_func}});
  MakeMemSink(map, "out");

  auto out = graph->Clone();
  EXPECT_OK(out.status());
  std::unique_ptr<IR> cloned_ir = out.ConsumeValueOrDie();

  ASSERT_EQ(graph->dag().TopologicalSort(), cloned_ir->dag().TopologicalSort());

  // Make sure that all of the columns are now part of the new graph.
  for (int64_t i : cloned_ir->dag().TopologicalSort()) {
    CompareClonedNodes(cloned_ir->Get(i), graph->Get(i), absl::Substitute("For index $0", i));
  }
}

TEST_F(CloneTests, all_op_clone) {
  auto mem_source = MakeMemSource();
  auto filter =
      MakeFilter(mem_source, MakeEqualsFunc(MakeMetadataIR("service", 0),
                                            MakeMetadataLiteral(MakeString("pl/test_service"))));
  auto limit = MakeLimit(filter, 10);

  auto agg = MakeBlockingAgg(limit, {MakeMetadataIR("service", 0)},
                             {{"mean", MakeMeanFunc(MakeColumn("equals_column", 0))}});
  auto map = MakeMap(agg, {{"mean_deux", MakeAddFunc(MakeColumn("mean", 0), MakeInt(3))},
                           {"mean", MakeColumn("mean", 0)}});
  MakeMemSink(map, "sup");
  auto out = graph->Clone();
  EXPECT_OK(out.status());
  std::unique_ptr<IR> cloned_ir = out.ConsumeValueOrDie();

  ASSERT_EQ(graph->dag().TopologicalSort(), cloned_ir->dag().TopologicalSort());

  // Make sure that all of the columns are now part of the new graph.
  for (int64_t i : cloned_ir->dag().TopologicalSort()) {
    CompareClonedNodes(cloned_ir->Get(i), graph->Get(i), absl::Substitute("For index $0", i));
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
  grpc_sink->SetDistributedID("agent-aa");
  grpc_sink->SetDestinationAddress("1111");

  EXPECT_OK(grpc_source->AddGRPCSink(grpc_sink));

  auto out = graph1->Clone();
  EXPECT_OK(out.status());
  std::unique_ptr<IR> cloned_ir1 = out.ConsumeValueOrDie();

  ASSERT_EQ(graph1->dag().TopologicalSort(), cloned_ir1->dag().TopologicalSort());

  // Make sure that all of the columns are now part of the new graph.
  for (int64_t i : cloned_ir1->dag().TopologicalSort()) {
    CompareClonedNodes(cloned_ir1->Get(i), graph1->Get(i), absl::Substitute("For index $0", i));
  }

  out = graph2->Clone();
  EXPECT_OK(out.status());
  std::unique_ptr<IR> cloned_ir2 = out.ConsumeValueOrDie();

  ASSERT_EQ(graph2->dag().TopologicalSort(), cloned_ir2->dag().TopologicalSort());

  // Make sure that all of the columns are now part of the new graph.
  for (int64_t i : cloned_ir2->dag().TopologicalSort()) {
    CompareClonedNodes(cloned_ir2->Get(i), graph2->Get(i), absl::Substitute("For index $0", i));
  }
}

TEST_F(CloneTests, grpc_source) {
  auto grpc_source = MakeGRPCSource("source_id/0", MakeRelation());
  MakeMemSink(grpc_source, "sup");
  auto out = graph->Clone();
  EXPECT_OK(out.status());
  std::unique_ptr<IR> cloned_ir = out.ConsumeValueOrDie();

  ASSERT_EQ(graph->dag().TopologicalSort(), cloned_ir->dag().TopologicalSort());

  // Make sure that all of the columns are now part of the new graph.
  for (int64_t i : cloned_ir->dag().TopologicalSort()) {
    CompareClonedNodes(cloned_ir->Get(i), graph->Get(i), absl::Substitute("For index $0", i));
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

  auto join_op = MakeJoin(
      {mem_src1, mem_src2}, "inner",
      MakeAndFunc(
          MakeEqualsFunc(MakeColumn("col1", 0, relation0), MakeColumn("col2", 1, relation1)),
          MakeEqualsFunc(MakeColumn("col3", 0, relation0), MakeColumn("col4", 1, relation1))),
      {{"left_only", MakeColumn("left_only", 0, relation0)},
       {"col4", MakeColumn("col4", 1, relation1)},
       {"col1", MakeColumn("col1", 0, relation0)},
       {"right_only", MakeColumn("right_only", 1, relation1)}});

  join_op->AddEqualityCondition(1, 2);
  join_op->AddEqualityCondition(3, 4);

  auto out = graph->Clone();

  EXPECT_OK(out.status());
  std::unique_ptr<IR> cloned_ir = out.ConsumeValueOrDie();
  ASSERT_EQ(graph->dag().TopologicalSort(), cloned_ir->dag().TopologicalSort());

  graph->Get(join_op->id());
  IRNode* maybe_join_clone = cloned_ir->Get(join_op->id());
  ASSERT_EQ(maybe_join_clone->type(), IRNodeType::kJoin);
  JoinIR* join_clone = static_cast<JoinIR*>(maybe_join_clone);

  CompareClonedJoin(join_clone, join_op, "");
}

class ToProtoTests : public OperatorTests {};
const char* kExpectedGRPCSourcePb = R"proto(
  op_type: GRPC_SOURCE_OPERATOR
  grpc_source_op {
    source_id: "$0"
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
  std::string source_id = "grpc_source_name";
  auto grpc_src = MakeGRPCSource(source_id, MakeRelation());
  MakeMemSink(grpc_src, "sink");

  planpb::Operator pb;
  ASSERT_OK(grpc_src->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(absl::Substitute(kExpectedGRPCSourcePb, source_id)));
}

const char* kExpectedGRPCSinkPb = R"proto(
  op_type: GRPC_SINK_OPERATOR
  grpc_sink_op {
    address: "$0"
    destination_id: "$1"
  }
)proto";

TEST_F(ToProtoTests, grpc_sink_ir) {
  int64_t destination_id = 123;
  std::string grpc_address = "1111";
  std::string physical_id = "agent-aa";
  auto mem_src = MakeMemSource();
  auto grpc_sink = MakeGRPCSink(mem_src, destination_id);
  grpc_sink->SetDistributedID(physical_id);
  grpc_sink->SetDestinationAddress(grpc_address);

  planpb::Operator pb;
  ASSERT_OK(grpc_sink->ToProto(&pb));

  EXPECT_THAT(
      pb, EqualsProto(absl::Substitute(kExpectedGRPCSinkPb, grpc_address,
                                       absl::Substitute("$0:$1", physical_id, destination_id))));
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
      sorted_children: 2
    }
    nodes {
      id: 2
      sorted_parents: 0
    }
  }
  nodes {
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
    id: 2
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

  auto join_op = MakeJoin(
      {mem_src1, mem_src2}, "inner",
      MakeAndFunc(
          MakeEqualsFunc(MakeColumn("col1", 0, relation0), MakeColumn("col2", 1, relation1)),
          MakeEqualsFunc(MakeColumn("col3", 0, relation0), MakeColumn("col4", 1, relation1))),
      {{"left_only", MakeColumn("left_only", 0, relation0)},
       {"col4", MakeColumn("col4", 1, relation1)},
       {"col1", MakeColumn("col1", 0, relation0)},
       {"right_only", MakeColumn("right_only", 1, relation1)}});

  join_op->AddEqualityCondition(1, 2);
  join_op->AddEqualityCondition(3, 4);

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

  auto join_op = MakeJoin(
      {mem_src1, mem_src2}, "left",
      MakeAndFunc(
          MakeEqualsFunc(MakeColumn("col1", 0, relation0), MakeColumn("col2", 1, relation1)),
          MakeEqualsFunc(MakeColumn("col3", 0, relation0), MakeColumn("col4", 1, relation1))),
      {{"left_only", MakeColumn("left_only", 0, relation0)},
       {"col4", MakeColumn("col4", 1, relation1)},
       {"col1", MakeColumn("col1", 0, relation0)},
       {"right_only", MakeColumn("right_only", 1, relation1)}});

  join_op->AddEqualityCondition(1, 2);
  join_op->AddEqualityCondition(3, 4);

  planpb::Operator pb;
  EXPECT_OK(join_op->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(kExpectedLeftJoinOpPb));
}

const char* kExpectedRightJoinOpPb = R"proto(
op_type: JOIN_OPERATOR
join_op {
  type: LEFT_OUTER
  equality_conditions {
    left_column_index: 2
    right_column_index: 1
  }
  equality_conditions {
    left_column_index: 4
    right_column_index: 3
  }
  output_columns {
    parent_index: 1
    column_index: 0
  }
  output_columns {
    parent_index: 0
    column_index: 4
  }
  output_columns {
    parent_index: 1
    column_index: 1
  }
  output_columns {
    parent_index: 0
    column_index: 0
  }
  column_names: "left_only"
  column_names: "col4"
  column_names: "col1"
  column_names: "right_only"
}
)proto";

TEST_F(ToProtoTests, right_join) {
  Relation relation0({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64},
                     {"left_only", "col1", "col2", "col3"});
  auto mem_src1 = MakeMemSource(relation0);

  Relation relation1({types::DataType::INT64, types::DataType::INT64, types::DataType::INT64,
                      types::DataType::INT64, types::DataType::INT64},
                     {"right_only", "col1", "col2", "col3", "col4"});
  auto mem_src2 = MakeMemSource(relation1);

  auto join_op = MakeJoin(
      {mem_src1, mem_src2}, "right",
      MakeAndFunc(
          MakeEqualsFunc(MakeColumn("col1", 0, relation0), MakeColumn("col2", 1, relation1)),
          MakeEqualsFunc(MakeColumn("col3", 0, relation0), MakeColumn("col4", 1, relation1))),
      {{"left_only", MakeColumn("left_only", 0, relation0)},
       {"col4", MakeColumn("col4", 1, relation1)},
       {"col1", MakeColumn("col1", 0, relation0)},
       {"right_only", MakeColumn("right_only", 1, relation1)}});

  join_op->AddEqualityCondition(2, 1);
  join_op->AddEqualityCondition(4, 3);

  planpb::Operator pb;
  EXPECT_OK(join_op->ToProto(&pb));

  EXPECT_EQ(join_op->parents()[0], mem_src2);
  EXPECT_EQ(join_op->parents()[1], mem_src1);

  EXPECT_THAT(pb, EqualsProto(kExpectedRightJoinOpPb));
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

  auto join_op = MakeJoin(
      {mem_src1, mem_src2}, "outer",
      MakeAndFunc(
          MakeEqualsFunc(MakeColumn("col1", 0, relation0), MakeColumn("col2", 1, relation1)),
          MakeEqualsFunc(MakeColumn("col3", 0, relation0), MakeColumn("col4", 1, relation1))),
      {{"left_only", MakeColumn("left_only", 0, relation0)},
       {"col4", MakeColumn("col4", 1, relation1)},
       {"col1", MakeColumn("col1", 0, relation0)},
       {"right_only", MakeColumn("right_only", 1, relation1)}});

  join_op->AddEqualityCondition(1, 2);
  join_op->AddEqualityCondition(3, 4);

  planpb::Operator pb;
  EXPECT_OK(join_op->ToProto(&pb));

  EXPECT_THAT(pb, EqualsProto(kExpectedOuterJoinOpPb));
}

TEST_F(OperatorTests, op_children) {
  auto mem_source = MakeMemSource();
  ColumnIR* col1 = MakeColumn("test1", 0);
  ColumnIR* col2 = MakeColumn("test2", 0);
  ColumnIR* col3 = MakeColumn("test3", 0);
  FuncIR* add_func = MakeAddFunc(col3, MakeInt(3));
  MapIR* map = MakeMap(mem_source, {{"out1", col1}, {"out2", col2}, {"out3", add_func}});
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
  MapIR* map = MakeMap(mem_source, {{"out1", col1}, {"out2", col2}, {"out3", add_func}});
  auto mem_sink = MakeMemSink(map, "out");

  EXPECT_THAT(graph->dag().nodes(), UnorderedElementsAre(0, 2, 3, 4, 5, 6, 7, 9));
  EXPECT_THAT(graph->dag().DependenciesOf(mem_source->id()), IsSupersetOf({map->id()}));
  EXPECT_THAT(graph->dag().ParentsOf(add_func->id()), IsSupersetOf({map->id()}));

  EXPECT_OK(graph->Prune({mem_sink->id(), map->id()}));
  EXPECT_THAT(graph->dag().nodes(), UnorderedElementsAre(0, 2, 3, 4, 5, 6));
  EXPECT_THAT(graph->dag().DependenciesOf(mem_source->id()), IsEmpty());
  EXPECT_THAT(graph->dag().ParentsOf(add_func->id()), IsEmpty());
}

TEST_F(OperatorTests, tablet_source_group) {
  auto mem_source = MakeMemSource("table", MakeRelation());
  auto tablet_source = graph->MakeNode<TabletSourceGroupIR>().ConsumeValueOrDie();
  EXPECT_OK(tablet_source->Init(mem_source, {"tablet1", "tablet2"}, "cpu0"));

  EXPECT_THAT(tablet_source->tablets(), ElementsAre("tablet1", "tablet2"));
  EXPECT_EQ(tablet_source->ReplacedMemorySource(), mem_source);
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
