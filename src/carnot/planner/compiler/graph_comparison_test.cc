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

#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <cstdint>

#include "src/carnot/planner/compiler/graph_comparison.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/testing/testing.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using testing::EqualsPlanGraph;
class GraphComparisonTest : public ::testing::Test {};

constexpr char kExpectedJoinPlan[] = R"proto(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 10
      sorted_children: 30
    }
    nodes {
      id: 20
      sorted_children: 30
    }
    nodes {
      id: 30
      sorted_children: 40
      sorted_parents: 10
      sorted_parents: 20
    }
    nodes {
      id: 40
      sorted_parents: 30
    }
  }
  nodes {
    id: 10
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
      }
    }
  }
  nodes {
    id: 20
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
      }
    }
  }
  nodes {
    id: 30
    op {
      op_type: JOIN_OPERATOR
      join_op {
      }
    }
  }
  nodes {
    id: 40
    op {
      op_type: GRPC_SINK_OPERATOR
      grpc_sink_op {
        address: "result_addr"
        output_table {
          table_name: "joined"
          column_types: UINT128
          column_types: FLOAT64
          column_names: "upid"
          column_names: "cpu0"
          column_semantic_types: ST_NONE
          column_semantic_types: ST_NONE
        }
        connection_options {
          ssl_targetname: "result_ssltarget"
        }
      }
    }
  }
}
)proto";

constexpr char kActualJoinPlan[] = R"proto(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 1
      sorted_children: 3
    }
    nodes {
      id: 2
      sorted_children: 3
    }
    nodes {
      id: 3
      sorted_children: 4
      sorted_parents: 1
      sorted_parents: 2
    }
    nodes {
      id: 4
      sorted_parents: 3
    }
  }
  nodes {
    id: 1
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
      }
    }
  }
  nodes {
    id: 2
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
      }
    }
  }
  nodes {
    id: 3
    op {
      op_type: JOIN_OPERATOR
      join_op {
        equality_conditions {
          left_column_index: 1
          right_column_index: 1
        }
        output_columns {
        }
        output_columns {
          column_index: 1
        }
        column_names: "cpu0"
        column_names: "upid"
      }
    }
  }
  nodes {
    id: 4
    op {
      op_type: GRPC_SINK_OPERATOR
      grpc_sink_op {
        address: "result_addr"
        output_table {
          table_name: "joined"
          column_types: UINT128
          column_types: FLOAT64
          column_names: "upid"
          column_names: "cpu0"
          column_semantic_types: ST_NONE
          column_semantic_types: ST_NONE
        }
        connection_options {
          ssl_targetname: "result_ssltarget"
        }
      }
    }
  }
}
)proto";
planpb::Plan ScramblePlanIDs(planpb::Plan plan) {
  auto mod_function = [](uint64_t id) { return id + 100; };
  auto fragment = plan.mutable_nodes(0);
  for (int idx = 0; idx < fragment->dag().nodes_size(); ++idx) {
    auto node = fragment->mutable_dag()->mutable_nodes(idx);
    auto old_sorted_children = node->sorted_children();
    auto old_sorted_parents = node->sorted_parents();
    node->clear_sorted_children();
    node->clear_sorted_parents();
    node->set_id((mod_function(node->id())));
    for (const auto& c : old_sorted_children) {
      node->add_sorted_children(mod_function(c));
    }
    for (const auto& c : old_sorted_parents) {
      node->add_sorted_parents(mod_function(c));
    }
  }
  for (int idx = 0; idx < fragment->nodes_size(); ++idx) {
    auto node = fragment->mutable_nodes(idx);
    node->set_id(mod_function(node->id()));
  }
  return plan;
}

TEST_F(GraphComparisonTest, simple_join) {
  // Expected and actual here differ by node ids.
  planpb::Plan expected_plan;
  google::protobuf::TextFormat::MergeFromString(kExpectedJoinPlan, &expected_plan);
  planpb::Plan actual_plan;
  google::protobuf::TextFormat::MergeFromString(kActualJoinPlan, &actual_plan);
  EXPECT_THAT(ScramblePlanIDs(expected_plan), EqualsPlanGraph(expected_plan));
}

TEST_F(GraphComparisonTest, change_join_op_types) {
  planpb::Plan expected_plan;
  google::protobuf::TextFormat::MergeFromString(kExpectedJoinPlan, &expected_plan);
  planpb::Plan actual_plan;
  google::protobuf::TextFormat::MergeFromString(kExpectedJoinPlan, &actual_plan);
  // Modify the source type of the join operator
  actual_plan.mutable_nodes(0)->mutable_nodes(0)->mutable_op()->set_op_type(
      ::px::carnot::planpb::OperatorType::GRPC_SOURCE_OPERATOR);
  EXPECT_THAT(actual_plan, ::testing::Not(EqualsPlanGraph(expected_plan)));

  actual_plan.Clear();
  google::protobuf::TextFormat::MergeFromString(kExpectedJoinPlan, &actual_plan);
  actual_plan.mutable_nodes(0)->mutable_nodes(2)->mutable_op()->set_op_type(
      ::px::carnot::planpb::OperatorType::UNION_OPERATOR);
  EXPECT_THAT(actual_plan, ::testing::Not(EqualsPlanGraph(expected_plan)));
}

constexpr char kExpectedTwoPathsPlan[] = R"proto(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 10
      sorted_children: 30
    }
    nodes {
      id: 20
      sorted_children: 40
    }
    nodes {
      id: 30
      sorted_parents: 10
    }
    nodes {
      id: 40
      sorted_parents: 20
    }
  }
  nodes {
    id: 10
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
      }
    }
  }
  nodes {
    id: 20
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
      }
    }
  }
  nodes {
    id: 30
    op {
      op_type: GRPC_SINK_OPERATOR
      grpc_sink_op {
        address: "result_addr"
        output_table {
          table_name: "joined"
          column_types: UINT128
          column_types: FLOAT64
          column_names: "upid"
          column_names: "cpu0"
          column_semantic_types: ST_NONE
          column_semantic_types: ST_NONE
        }
        connection_options {
          ssl_targetname: "result_ssltarget"
        }
      }
    }
  }
  nodes {
    id: 40
    op {
      op_type: GRPC_SINK_OPERATOR
      grpc_sink_op {
        address: "result_addr"
        output_table {
          table_name: "joined"
          column_types: UINT128
          column_types: FLOAT64
          column_names: "upid"
          column_names: "cpu0"
          column_semantic_types: ST_NONE
          column_semantic_types: ST_NONE
        }
        connection_options {
          ssl_targetname: "result_ssltarget"
        }
      }
    }
  }
}
)proto";

TEST_F(GraphComparisonTest, compare_join_to_two_paths) {
  planpb::Plan expected_plan;
  google::protobuf::TextFormat::MergeFromString(kExpectedJoinPlan, &expected_plan);
  planpb::Plan actual_plan;
  google::protobuf::TextFormat::MergeFromString(kExpectedTwoPathsPlan, &actual_plan);
  EXPECT_THAT(actual_plan, ::testing::Not(EqualsPlanGraph(expected_plan)));
  EXPECT_THAT(ScramblePlanIDs(actual_plan), EqualsPlanGraph(actual_plan));
}

constexpr char kExpectedTwoChildren[] = R"proto(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 10
      sorted_children: 20
      sorted_children: 40
    }
    nodes {
      id: 20
      sorted_parents: 10
    }
    nodes {
      id: 30
      sorted_parents: 20
    }
    nodes {
      id: 40
      sorted_parents: 10
    }
  }
  nodes {
    id: 10
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
      }
    }
  }
  nodes {
    id: 20
    op {
      op_type: MAP_OPERATOR
      map_op {}
    }
  }
  nodes {
    id: 30
    op {
      op_type: GRPC_SINK_OPERATOR
      grpc_sink_op {
        address: "result_addr"
        output_table {
          table_name: "joined"
          column_types: UINT128
          column_types: FLOAT64
          column_names: "upid"
          column_names: "cpu0"
          column_semantic_types: ST_NONE
          column_semantic_types: ST_NONE
        }
        connection_options {
          ssl_targetname: "result_ssltarget"
        }
      }
    }
  }
  nodes {
    id: 40
    op {
      op_type: GRPC_SINK_OPERATOR
      grpc_sink_op {
        address: "result_addr"
        output_table {
          table_name: "joined"
          column_types: UINT128
          column_types: FLOAT64
          column_names: "upid"
          column_names: "cpu0"
          column_semantic_types: ST_NONE
          column_semantic_types: ST_NONE
        }
        connection_options {
          ssl_targetname: "result_ssltarget"
        }
      }
    }
  }
}
)proto";

TEST_F(GraphComparisonTest, two_children) {
  planpb::Plan expected_plan;
  google::protobuf::TextFormat::MergeFromString(kExpectedJoinPlan, &expected_plan);
  planpb::Plan actual_plan;
  google::protobuf::TextFormat::MergeFromString(kExpectedTwoChildren, &actual_plan);
  EXPECT_THAT(actual_plan, ::testing::Not(EqualsPlanGraph(expected_plan)));
  EXPECT_THAT(ScramblePlanIDs(actual_plan), EqualsPlanGraph(actual_plan));
}

constexpr char kThreeParents[] = R"proto(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 10
      sorted_children: 30
    }
    nodes {
      id: 11
      sorted_children: 30
    }
    nodes {
      id: 12
      sorted_children: 30
    }
    nodes {
      id: 30
      sorted_parents: 10
      sorted_parents: 11
      sorted_parents: 12
    }
    nodes {
      id: 40
      sorted_parents: 30
    }
  }
  nodes {
    id: 10
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "process_stats"
      }
    }
  }
  nodes {
    id: 11
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "http_events"
      }
    }
  }
  nodes {
    id: 12
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "cpu"
      }
    }
  }
  nodes {
    id: 30
    op {
      op_type: UNION_OPERATOR
      union_op {
      }
    }
  }
  nodes {
    id: 40
    op {
      op_type: GRPC_SINK_OPERATOR
      grpc_sink_op {
        address: "result_addr"
        output_table {
          table_name: "joined"
          column_types: UINT128
          column_types: FLOAT64
          column_names: "upid"
          column_names: "cpu0"
          column_semantic_types: ST_NONE
          column_semantic_types: ST_NONE
        }
        connection_options {
          ssl_targetname: "result_ssltarget"
        }
      }
    }
  }
}
)proto";

TEST_F(GraphComparisonTest, three_parents) {
  planpb::Plan three_parents_plan;
  google::protobuf::TextFormat::MergeFromString(kThreeParents, &three_parents_plan);
  planpb::Plan join_plan;
  google::protobuf::TextFormat::MergeFromString(kExpectedJoinPlan, &join_plan);
  planpb::Plan two_paths_plan;
  google::protobuf::TextFormat::MergeFromString(kExpectedTwoPathsPlan, &two_paths_plan);
  EXPECT_THAT(three_parents_plan, ::testing::Not(EqualsPlanGraph(join_plan)));
  EXPECT_THAT(three_parents_plan, ::testing::Not(EqualsPlanGraph(two_paths_plan)));
  EXPECT_THAT(ScramblePlanIDs(three_parents_plan), EqualsPlanGraph(three_parents_plan));
}

constexpr char kSingleStrandPlan[] = R"proto(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 10
      sorted_children: 20
    }
    nodes {
      id: 20
      sorted_parents: 10
      sorted_children: 30
    }
    nodes {
      id: 30
      sorted_parents: 20
    }
  }
  nodes {
    id: 10
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "process_stats"
      }
    }
  }
  nodes {
    id: 30
    op {
      $0
    }
  }
  nodes {
    id: 40
    op {
      op_type: GRPC_SINK_OPERATOR
      grpc_sink_op {
        address: "result_addr"
        output_table {
          table_name: "joined"
          column_types: UINT128
          column_types: FLOAT64
          column_names: "upid"
          column_names: "cpu0"
          column_semantic_types: ST_NONE
          column_semantic_types: ST_NONE
        }
        connection_options {
          ssl_targetname: "result_ssltarget"
        }
      }
    }
  }
}
)proto";

TEST_F(GraphComparisonTest, ignore_column_node_values) {
  planpb::Plan expected_plan;
  google::protobuf::TextFormat::MergeFromString(absl::Substitute(kSingleStrandPlan, R"proto(
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 20
          }
        }
      }
    )proto"),
                                                &expected_plan);
  planpb::Plan actual_plan;
  google::protobuf::TextFormat::MergeFromString(absl::Substitute(kSingleStrandPlan, R"proto(
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          column {
            node: 30
          }
        }
      }
    )proto"),
                                                &actual_plan);
  EXPECT_THAT(ScramblePlanIDs(actual_plan), EqualsPlanGraph(expected_plan));
}

constexpr char kThreePathsPlan[] = R"proto(
dag {
  nodes {
    id: 1
  }
}
nodes {
  id: 1
  dag {
    nodes {
      id: 10
      sorted_children: 30
    }
    nodes {
      id: 20
      sorted_children: 40
    }
    nodes {
      id: 50
      sorted_children: 60
    }
    nodes {
      id: 30
      sorted_parents: 10
    }
    nodes {
      id: 40
      sorted_parents: 20
    }
    nodes {
      id: 60
      sorted_parents: 50
    }
  }
  nodes {
    id: 10
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
      }
    }
  }
  nodes {
    id: 20
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
      }
    }
  }
  nodes {
    id: 50
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
      }
    }
  }
  nodes {
    id: 30
    op {
      op_type: GRPC_SINK_OPERATOR
      grpc_sink_op {
      }
    }
  }
  nodes {
    id: 40
    op {
      op_type: GRPC_SINK_OPERATOR
      grpc_sink_op {
      }
    }
  }
  nodes {
    id: 60
    op {
      op_type: GRPC_SINK_OPERATOR
      grpc_sink_op {
      }
    }
  }
}
)proto";

TEST_F(GraphComparisonTest, three_paths_not_transitive) {
  // Test for assumption that subgraph equality is transitive. Because
  // of how PARTIAL proto differencer works, subgraph equality is not transitive.
  // Let’s take 2 graphs where the actual has roots A, B, C and the expected has roots 1, 2, 3
  // ie
  // A is MemSrc(table="a") B is MemSrc(table="b") C is MemSrc(table="c")
  // 1 is MemSrc() no tablename aka wildcard tablename
  // 2 is MemSrc(table="b") 3 is MemSrc(table="b")

  // Compare Roots comes up with the following associations.
  // A==1 B==1 C==1
  // B==2
  // B==3

  // If the impl assumes transitivity then it would incorrectly determine that A==2 and impl
  // might wrongly assume actual matches expected.

  planpb::Plan expected_plan;
  google::protobuf::TextFormat::MergeFromString(kThreePathsPlan, &expected_plan);
  expected_plan.mutable_nodes(0)->mutable_nodes(0)->mutable_op()->mutable_mem_source_op()->set_name(
      "b");
  expected_plan.mutable_nodes(0)->mutable_nodes(1)->mutable_op()->mutable_mem_source_op()->set_name(
      "b");
  expected_plan.mutable_nodes(0)->mutable_nodes(2)->mutable_op()->mutable_mem_source_op()->set_name(
      "");

  // Copy the plan over and modify it.
  planpb::Plan actual_plan;
  actual_plan.CopyFrom(expected_plan);
  actual_plan.mutable_nodes(0)->mutable_nodes(0)->mutable_op()->mutable_mem_source_op()->set_name(
      "a");
  actual_plan.mutable_nodes(0)->mutable_nodes(1)->mutable_op()->mutable_mem_source_op()->set_name(
      "b");
  actual_plan.mutable_nodes(0)->mutable_nodes(2)->mutable_op()->mutable_mem_source_op()->set_name(
      "c");
  EXPECT_THAT(actual_plan, ::testing::Not(EqualsPlanGraph(expected_plan)));

  // Double check that we're not failing for other reasons by making expected_plan match
  // actual_plan.
  expected_plan.mutable_nodes(0)->mutable_nodes(0)->mutable_op()->mutable_mem_source_op()->set_name(
      "b");
  expected_plan.mutable_nodes(0)->mutable_nodes(1)->mutable_op()->mutable_mem_source_op()->set_name(
      "");
  expected_plan.mutable_nodes(0)->mutable_nodes(2)->mutable_op()->mutable_mem_source_op()->set_name(
      "");
  EXPECT_THAT(actual_plan, EqualsPlanGraph(expected_plan));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
