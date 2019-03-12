#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <unordered_map>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/compiler.h"
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/ir_relation_handler.h"
#include "src/carnot/compiler/test_utils.h"

namespace pl {
namespace carnot {
namespace compiler {

using testing::_;

const char* kExpectedUDFInfo = R"(
scalar_udfs {
  name: "pl.divide"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type:FLOAT64
}
scalar_udfs {
  name: "pl.add"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type:  FLOAT64
}
scalar_udfs {
  name: "pl.multiply"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type:  FLOAT64
}
scalar_udfs {
  name: "pl.subtract"
  exec_arg_types: FLOAT64
  exec_arg_types: FLOAT64
  return_type:  FLOAT64
}
udas {
  name: "pl.count"
  update_arg_types: FLOAT64
  finalize_type:  INT64
}
udas {
  name: "pl.count"
  update_arg_types: INT64
  finalize_type:  INT64
}
udas {
  name: "pl.count"
  update_arg_types: BOOLEAN
  finalize_type:  INT64
}
udas {
  name: "pl.count"
  update_arg_types: STRING
  finalize_type:  INT64
}
udas {
  name: "pl.mean"
  update_arg_types: FLOAT64
  finalize_type:  FLOAT64
}
)";

class RelationHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();

    registry_info_ = std::make_shared<RegistryInfo>();
    carnotpb::UDFInfo info_pb;
    google::protobuf::TextFormat::MergeFromString(kExpectedUDFInfo, &info_pb);
    EXPECT_OK(registry_info_->Init(info_pb));
    plan::Relation cpu_relation;
    cpu_relation.AddColumn(types::FLOAT64, "cpu0");
    cpu_relation.AddColumn(types::FLOAT64, "cpu1");
    cpu_relation.AddColumn(types::FLOAT64, "cpu2");
    relation_map_.emplace("cpu", cpu_relation);

    plan::Relation non_float_relation;
    non_float_relation.AddColumn(types::INT64, "int_col");
    non_float_relation.AddColumn(types::FLOAT64, "float_col");
    non_float_relation.AddColumn(types::STRING, "string_col");
    non_float_relation.AddColumn(types::BOOLEAN, "bool_col");
    relation_map_.emplace("non_float_table", non_float_relation);
  }

  StatusOr<std::shared_ptr<IR>> CompileGraph(const std::string& query) {
    auto result = ParseQuery(query);
    PL_RETURN_IF_ERROR(result);
    // just a quick test to find issues.
    if (!result.ValueOrDie()->GetSink().ok()) {
      return error::InvalidArgument("IR Doesn't have sink");
    }
    return result;
  }
  Status HandleRelation(std::shared_ptr<IR> ir_graph) {
    auto relation_handler = IRRelationHandler(relation_map_, *registry_info_);
    return relation_handler.UpdateRelationsAndCheckFunctions(ir_graph.get());
  }
  bool RelationEquality(const plan::Relation& r1, const plan::Relation& r2) {
    std::vector<std::string> r1_names;
    std::vector<std::string> r2_names;
    std::vector<types::DataType> r1_types;
    std::vector<types::DataType> r2_types;
    if (r1.NumColumns() >= r2.NumColumns()) {
      r1_names = r1.col_names();
      r1_types = r1.col_types();
      r2_names = r2.col_names();
      r2_types = r2.col_types();
    } else {
      r1_names = r2.col_names();
      r1_types = r2.col_types();
      r2_names = r1.col_names();
      r2_types = r1.col_types();
    }
    for (size_t i = 0; i < r1_names.size(); i++) {
      std::string col1 = r1_names[i];
      auto type1 = r1_types[i];
      auto r2_iter = std::find(r2_names.begin(), r2_names.end(), col1);
      // if we can't find name in the second relation, then
      if (r2_iter == r2_names.end()) return false;
      int64_t r2_idx = std::distance(r2_names.begin(), r2_iter);
      if (r2_types[r2_idx] != type1) return false;
    }
    return true;
  }

  /**
   * @brief Finds the specified type in the graph and returns the node.
   *
   *
   * @param ir_graph
   * @param type
   * @return StatusOr<IRNode*> IRNode of type, otherwise returns an error.
   */
  StatusOr<IRNode*> FindNodeType(std::shared_ptr<IR> ir_graph, IRNodeType type) {
    for (auto& i : ir_graph->dag().TopologicalSort()) {
      auto node = ir_graph->Get(i);
      if (node->type() == type) {
        return node;
      }
    }
    return error::NotFound("Couldn't find node of type $0 in ir_graph.", kIRNodeStrings[type]);
  }

  // std::unique_ptr<CompilerState> compiler_state_;
  std::shared_ptr<RegistryInfo> registry_info_;
  std::unordered_map<std::string, plan::Relation> relation_map_;
};

TEST_F(RelationHandlerTest, test_utils) {
  plan::Relation cpu2_relation;
  cpu2_relation.AddColumn(types::FLOAT64, "cpu0");
  cpu2_relation.AddColumn(types::FLOAT64, "cpu1");
  EXPECT_FALSE(RelationEquality(relation_map_["cpu"], cpu2_relation));
  EXPECT_TRUE(RelationEquality(relation_map_["cpu"], relation_map_["cpu"]));
}

TEST_F(RelationHandlerTest, no_special_relation) {
  std::string from_expr = "From(table='cpu', select=['cpu0', 'cpu1']).Result(name='cpu')";
  auto ir_graph_status = CompileGraph(from_expr);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  // check the connection of ig
  std::string from_range_expr =
      "From(table='cpu', select=['cpu0']).Range(time='-2m').Result(name='cpu_out')";
  ir_graph_status = CompileGraph(from_expr);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();
}

TEST_F(RelationHandlerTest, assign_functionality) {
  std::string assign_and_use =
      absl::StrJoin({"queryDF = From(table = 'cpu', select = [ 'cpu0', 'cpu1' ])",
                     "queryDF.Range(time ='-2m').Result(name='cpu_out')"},
                    "\n");

  auto ir_graph_status = CompileGraph(assign_and_use);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();
}

// Map Tests
TEST_F(RelationHandlerTest, single_col_map) {
  std::string single_col_map_sum =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
                     "mapDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1})",
                     "mapDF.Result(name='cpu_out')"},
                    "\n");
  auto ir_graph_status = CompileGraph(single_col_map_sum);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  std::string single_col_div_map_query =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
                     "mapDF = queryDF.Map(fn=lambda r : {'sum' : pl.divide(r.cpu0,r.cpu1)})",
                     "mapDF.Result(name='cpu_out')"},
                    "\n");
  ir_graph_status = CompileGraph(single_col_div_map_query);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();
}

TEST_F(RelationHandlerTest, multi_col_map) {
  std::string multi_col = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(time='-2m')",
          "mapDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1, 'copy' : r.cpu2})",
          "mapDF.Result(name='cpu_out')",
      },
      "\n");
  auto ir_graph_status = CompileGraph(multi_col);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();
}

TEST_F(RelationHandlerTest, bin_op_test) {
  std::string single_col_map_sum =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
                     "mapDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1})",
                     "mapDF.Result(name='cpu_out')"},
                    "\n");
  auto ir_graph_status = CompileGraph(single_col_map_sum);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  std::string single_col_map_sub =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
                     "mapDF = queryDF.Map(fn=lambda r : {'sub' : r.cpu0 - r.cpu1})",
                     "mapDF.Result(name='cpu_out')"},
                    "\n");
  ir_graph_status = CompileGraph(single_col_map_sub);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  std::string single_col_map_product =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
                     "mapDF = queryDF.Map(fn=lambda r : {'product' : r.cpu0 * r.cpu1})",
                     "mapDF.Result(name='cpu_out')"},
                    "\n");
  ir_graph_status = CompileGraph(single_col_map_product);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  std::string single_col_map_quotient =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
                     "mapDF = queryDF.Map(fn=lambda r : {'quotient' : r.cpu0 / r.cpu1})",
                     "mapDF.Result(name='cpu_out')"},
                    "\n");
  ir_graph_status = CompileGraph(single_col_map_quotient);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();
}

TEST_F(RelationHandlerTest, single_col_agg) {
  std::string single_col_agg =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
                     "aggDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
                     "pl.count(r.cpu1)}).Result(name='cpu_out')"},
                    "\n");
  auto ir_graph_status = CompileGraph(single_col_agg);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  auto handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();
  //   GraphVerify(single_col_agg, false);
  std::string multi_output_col_agg =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0','cpu1']).Range(time='-2m')",
                     "aggDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count': "
                     "pl.count(r.cpu1), 'cpu_mean' : pl.mean(r.cpu1)}).Result(name='cpu_out')"},
                    "\n");
  ir_graph_status = CompileGraph(multi_output_col_agg);
  ASSERT_OK(ir_graph_status);
  // now pass into the relation handler.
  handle_status = HandleRelation(ir_graph_status.ConsumeValueOrDie());
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();
}

// Make sure the relations match the expected values.
TEST_F(RelationHandlerTest, test_relation_results) {
  // operators don't use generated columns, are just chained.
  std::string chain_operators = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(time='-2m')",
       "mapDF = queryDF.Map(fn=lambda r : {'cpu0' : r.cpu0, 'cpu1' : r.cpu1, 'cpu_sum' : "
       "r.cpu0+r.cpu1})",
       "aggDF = mapDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
       "pl.count(r.cpu1), 'cpu_mean' : pl.mean(r.cpu1)}).Result(name='cpu_out')"},
      "\n");
  auto ir_graph_status = CompileGraph(chain_operators);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);

  // Memory Source should copy the source relation.
  auto source_node_status = FindNodeType(ir_graph, MemorySourceType);
  EXPECT_OK(source_node_status);
  auto source_node = static_cast<MemorySourceIR*>(source_node_status.ConsumeValueOrDie());
  EXPECT_TRUE(RelationEquality(source_node->relation(), relation_map_["cpu"]));
  auto mem_node_status = FindNodeType(ir_graph, MemorySinkType);

  // Map relation should be contain cpu0, cpu1, and cpu_sum.
  auto map_node_status = FindNodeType(ir_graph, MapType);
  EXPECT_OK(map_node_status);
  auto map_node = static_cast<MapIR*>(map_node_status.ConsumeValueOrDie());
  auto test_map_relation_s = relation_map_["cpu"].MakeSubRelation({"cpu0", "cpu1"});
  EXPECT_OK(test_map_relation_s);
  plan::Relation test_map_relation = test_map_relation_s.ConsumeValueOrDie();
  test_map_relation.AddColumn(types::FLOAT64, "cpu_sum");
  EXPECT_TRUE(RelationEquality(map_node->relation(), test_map_relation));

  // Agg should be a new relation with one column.
  auto agg_node_status = FindNodeType(ir_graph, BlockingAggType);
  EXPECT_OK(agg_node_status);
  auto agg_node = static_cast<BlockingAggIR*>(agg_node_status.ConsumeValueOrDie());
  plan::Relation test_agg_relation;
  test_agg_relation.AddColumn(types::INT64, "cpu_count");
  test_agg_relation.AddColumn(types::FLOAT64, "cpu_mean");
  test_agg_relation.AddColumn(types::FLOAT64, "cpu0");
  EXPECT_TRUE(RelationEquality(agg_node->relation(), test_agg_relation));

  // Sink should have the same relation as before and be equivalent to its parent.
  auto sink_node_status = FindNodeType(ir_graph, MemorySinkType);
  EXPECT_OK(sink_node_status);
  auto sink_node = static_cast<MemorySinkIR*>(sink_node_status.ConsumeValueOrDie());
  EXPECT_TRUE(RelationEquality(sink_node->relation(), test_agg_relation));
  EXPECT_TRUE(RelationEquality(sink_node->relation(), sink_node->parent()->relation()));
}  // namespace compiler

// Make sure the compiler exits when calling columns that aren't explicitly called.
TEST_F(RelationHandlerTest, test_relation_fails) {
  // operators don't use generated columns, are just chained.
  std::string chain_operators = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(time='-2m')",
       "mapDF = queryDF.Map(fn=lambda r : {'cpu_sum' : r.cpu0+r.cpu1})",
       "aggDF = mapDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
       "pl.count(r.cpu1), 'cpu_mean' : pl.mean(r.cpu1)}).Result(name='cpu_out')"},
      "\n");
  auto ir_graph_status = CompileGraph(chain_operators);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();

  // This query assumes implicit copying of Input relation into Map. The relation handler should
  // fail.
  auto handle_status = HandleRelation(ir_graph);
  VLOG(1) << handle_status.ToString();
  EXPECT_FALSE(handle_status.ok());

  // Map should result just be the cpu_sum column.
  auto map_node_status = FindNodeType(ir_graph, MapType);
  EXPECT_OK(map_node_status);
  auto map_node = static_cast<MapIR*>(map_node_status.ConsumeValueOrDie());
  plan::Relation test_map_relation;
  test_map_relation.AddColumn(types::FLOAT64, "cpu_sum");
  EXPECT_TRUE(RelationEquality(map_node->relation(), test_map_relation));
}

TEST_F(RelationHandlerTest, test_relation_multi_col_agg) {
  std::string chain_operators = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(time='-2m')",
       "aggDF = queryDF.Agg(by=lambda r : [r.cpu0, r.cpu2], fn=lambda r : {'cpu_count' : "
       "pl.count(r.cpu1), 'cpu_mean' : pl.mean(r.cpu1)}).Result(name='cpu_out')"},
      "\n");
  auto ir_graph_status = CompileGraph(chain_operators);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  VLOG(1) << handle_status.ToString();
  ASSERT_OK(handle_status);

  auto agg_node_status = FindNodeType(ir_graph, BlockingAggType);
  EXPECT_OK(agg_node_status);
  auto agg_node = static_cast<BlockingAggIR*>(agg_node_status.ConsumeValueOrDie());
  plan::Relation test_agg_relation;
  test_agg_relation.AddColumn(types::INT64, "cpu_count");
  test_agg_relation.AddColumn(types::FLOAT64, "cpu_mean");
  test_agg_relation.AddColumn(types::FLOAT64, "cpu0");
  test_agg_relation.AddColumn(types::FLOAT64, "cpu2");
  EXPECT_TRUE(RelationEquality(agg_node->relation(), test_agg_relation));
}

TEST_F(RelationHandlerTest, test_from_select) {
  // operators don't use generated columns, are just chained.
  std::string chain_operators =
      "queryDF = From(table='cpu', select=['cpu0', "
      "'cpu2']).Range(time='-2m').Result(name='cpu_out')";
  plan::Relation test_relation;
  test_relation.AddColumn(types::FLOAT64, "cpu0");
  test_relation.AddColumn(types::FLOAT64, "cpu2");
  auto ir_graph_status = CompileGraph(chain_operators);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  auto sink_node_status = FindNodeType(ir_graph, MemorySinkType);
  EXPECT_OK(sink_node_status);
  auto sink_node = static_cast<MemorySinkIR*>(sink_node_status.ConsumeValueOrDie());
  EXPECT_TRUE(RelationEquality(sink_node->relation(), test_relation));
}

// Test to make sure the system detects udfs/udas that don't exist.
TEST_F(RelationHandlerTest, nonexistant_udfs) {
  std::string missing_udf =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
                     "mapDF = queryDF.Map(fn=lambda r : {'cpu_sum' : "
                     "pl.sus(r.cpu0,r.cpu1)}).Result(name='cpu_out')"},
                    "\n");

  auto ir_graph_status = CompileGraph(missing_udf);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  EXPECT_FALSE(handle_status.ok());
  std::string missing_uda =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
                     "aggDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
                     "pl.punt(r.cpu1)}).Result(name='cpu_out')"},
                    "\n");

  ir_graph_status = CompileGraph(missing_uda);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  EXPECT_FALSE(handle_status.ok());
}

TEST_F(RelationHandlerTest, nonexistant_cols) {
  // Test for columns used in map function that don't exist in relation.
  std::string wrong_column_map_func =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
                     "mapDF = queryDF.Map(fn=lambda r : {'cpu_sum' : "
                     "pl.sum(r.cpu0,r.cpu100)}).Result(name='cpu_out')"},
                    "\n");

  auto ir_graph_status = CompileGraph(wrong_column_map_func);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  EXPECT_FALSE(handle_status.ok());
  VLOG(1) << handle_status.status().ToString();

  // Test for columns used in group_by arg of Agg that don't exist.
  std::string wrong_column_agg_by =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
                     "aggDF = queryDF.Agg(by=lambda r : r.cpu101, fn=lambda r : {'cpu_count' "
                     ": "
                     "pl.count(r.cpu1)}).Result(name='cpu_out')"},
                    "\n");
  ir_graph_status = CompileGraph(wrong_column_agg_by);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  EXPECT_FALSE(handle_status.ok());
  VLOG(1) << handle_status.status().ToString();

  // Test for column not selected in From.
  std::string not_selected_col =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu2']).Range(time='-2m')",
                     "aggDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : "
                     "pl.count(r.cpu1)}).Result(name='cpu_out')"},
                    "\n");
  ir_graph_status = CompileGraph(not_selected_col);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  EXPECT_FALSE(handle_status.ok());
  VLOG(1) << handle_status.status().ToString();
}

// Use results of created columns in later parts of the pipeline.
TEST_F(RelationHandlerTest, created_columns) {
  std::string agg_use_map_col_fn = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(time='-2m')",
       "mapDF = queryDF.Map(fn=lambda r : {'cpu2' : r.cpu2, 'cpu_sum' : r.cpu0+r.cpu1})",
       "aggDF = mapDF.Agg(by=lambda r : r.cpu2, fn=lambda r : {'cpu_count' : "
       "pl.count(r.cpu_sum)}).Result(name='cpu_out')"},
      "\n");
  auto ir_graph_status = CompileGraph(agg_use_map_col_fn);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  std::string agg_use_map_col_by = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(time='-2m')",
       "mapDF = queryDF.Map(fn=lambda r : {'cpu2' : r.cpu2, 'cpu_sum' : r.cpu0+r.cpu1})",
       "aggDF = mapDF.Agg(by=lambda r : r.cpu_sum, fn=lambda r : {'cpu_count' : "
       "pl.count(r.cpu2)}).Result(name='cpu_out')"},
      "\n");
  ir_graph_status = CompileGraph(agg_use_map_col_by);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  std::string map_use_agg_col = absl::StrJoin(
      {
          "queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(time='-2m')",
          "aggDF = queryDF.Agg(by=lambda r : r.cpu1, fn=lambda r : {'cpu0_mean' : "
          "pl.mean(r.cpu0), "
          "'cpu1_mean' : pl.mean(r.cpu1)})",
          "mapDF = aggDF.Map(fn=lambda r : {'cpu_sum' : "
          "r.cpu1_mean+r.cpu1_mean}).Result(name='cpu_out')",
      },
      "\n");
  ir_graph_status = CompileGraph(map_use_agg_col);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  std::string map_use_map_col = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(time='-2m')",
       "mapDF = queryDF.Map(fn=lambda r : {'cpu2': r.cpu2, 'cpu_sum' : r.cpu0+r.cpu1})",
       "map2Df = mapDF.Map(fn=lambda r : {'cpu_sum2' : r.cpu2+r.cpu_sum}).Result(name='cpu_out')"},
      "\n");
  ir_graph_status = CompileGraph(map_use_map_col);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  std::string agg_use_agg_col = absl::StrJoin(
      {"queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(time='-2m')",
       "aggDF = queryDF.Agg(by=lambda r : r.cpu1, fn=lambda r : {'cpu0_mean' : pl.mean(r.cpu0), "
       "'cpu1_mean' : pl.mean(r.cpu1)})",
       "agg2DF = aggDF.Agg(by=lambda r : r.cpu1_mean, fn=lambda r : {'cpu0_mean_mean' : "
       "pl.mean(r.cpu0_mean)}).Result(name='cpu_out') "},
      "\n");
  ir_graph_status = CompileGraph(agg_use_agg_col);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();
}

TEST_F(RelationHandlerTest, non_float_columns) {
  std::string agg_fn_count_all = absl::StrJoin(
      {
          "queryDF = From(table='non_float_table', select=['float_col', 'int_col', 'bool_col', "
          "'string_col']).Range(time='-2m')",
          "aggDF = queryDF.Agg(by=lambda r : r.float_col, fn=lambda r : {"
          "'int_count' : pl.count(r.int_col), "
          "'bool_count' : pl.count(r.bool_col),"
          " 'string_count' : pl.count(r.string_col)}).Result(name='cpu_out')",
      },
      "\n");
  auto ir_graph_status = CompileGraph(agg_fn_count_all);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();

  std::string by_fn_count_all = absl::StrJoin(
      {
          "queryDF = From(table='non_float_table', select=['float_col', 'int_col', 'bool_col', "
          "'string_col']).Range(time='-2m')",
          "aggDF = queryDF.Agg(by=lambda r : r.int_col, fn=lambda r : {"
          "'float_count' : pl.count(r.float_col), "
          "'bool_count' : pl.count(r.bool_col),"
          " 'string_count' : pl.count(r.string_col)}).Result(name='cpu_out')",
      },
      "\n");
  ir_graph_status = CompileGraph(by_fn_count_all);
  ASSERT_OK(ir_graph_status);
  ir_graph = ir_graph_status.ConsumeValueOrDie();
  handle_status = HandleRelation(ir_graph);
  EXPECT_OK(handle_status);
  VLOG(1) << handle_status.status().ToString();
}

TEST_F(RelationHandlerTest, add_pl_time_type_fail) {
  std::string time_fail_float_time_add =
      absl::StrJoin({"queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')",
                     "mapDF = queryDF.Map(fn=lambda r : {'sub' : r.cpu0 + pl.second})",
                     "mapDF.Result(name='cpu_out')"},
                    "\n");
  auto ir_graph_status = CompileGraph(time_fail_float_time_add);
  ASSERT_OK(ir_graph_status);
  auto ir_graph = ir_graph_status.ConsumeValueOrDie();
  auto handle_status = HandleRelation(ir_graph);
  EXPECT_FALSE(handle_status.ok());
  VLOG(1) << handle_status.status().ToString();
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
