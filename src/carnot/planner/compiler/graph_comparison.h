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

#pragma once

#include <utility>
#include <vector>

#include <cstdio>
#include <fstream>
#include <memory>
#include <queue>
#include <regex>
#include <string>
#include <unordered_set>

#include <absl/container/flat_hash_set.h>
#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compiler/ast_visitor.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/distributedpb/distributed_plan.pb.h"
#include "src/carnot/planner/metadata/metadata_handler.h"
#include "src/carnot/planner/parser/parser.h"
#include "src/carnot/planner/parser/string_reader.h"
#include "src/carnot/planner/probes/tracing_module.h"
#include "src/carnot/planner/rules/rules.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/testing/testing.h"
#include "src/shared/types/typespb/types.pb.h"

namespace px {
namespace carnot {
namespace planner {
namespace testing {

struct PlanGraphMatcher {
  explicit PlanGraphMatcher(planpb::Plan plan) : expected_plan_(std::move(plan)) {}
  struct PlanNodeData {
    planpb::PlanNode pb;
    std::vector<uint64_t> parents;
    std::vector<uint64_t> children;

    std::string DebugString() const { return pb.DebugString(); }
  };
  struct Graph {
    absl::flat_hash_map<uint64_t, PlanNodeData> plan_map;
    std::vector<uint64_t> root_nodes;
  };
  Graph BuildGraph(const planpb::Plan& plan) const {
    Graph graph;
    CHECK_EQ(plan.nodes_size(), 1);
    auto plan_fragment = plan.nodes()[0];
    for (const auto& node : plan_fragment.nodes()) {
      CHECK(!graph.plan_map.contains(node.id()));
      graph.plan_map[node.id()].pb = node;
    }
    for (const auto& dag_node : plan_fragment.dag().nodes()) {
      if (!dag_node.sorted_parents_size()) {
        graph.root_nodes.push_back(dag_node.id());
      }
      graph.plan_map[dag_node.id()].parents =
          std::vector<uint64_t>{dag_node.sorted_parents().begin(), dag_node.sorted_parents().end()};
      graph.plan_map[dag_node.id()].children = std::vector<uint64_t>{
          dag_node.sorted_children().begin(), dag_node.sorted_children().end()};
    }
    return graph;
  }

  bool CompareNodes(const PlanNodeData& expected_node, const PlanNodeData& actual_node) const {
    if (expected_node.parents.size() != actual_node.parents.size()) {
      return false;
    }
    if (expected_node.children.size() != actual_node.children.size()) {
      return false;
    }
    google::protobuf::util::MessageDifferencer differencer_;
    differencer_.set_scope(google::protobuf::util::MessageDifferencer::Scope::PARTIAL);
    differencer_.IgnoreField(planpb::Column::descriptor()->FindFieldByName("node"));
    differencer_.IgnoreField(
        planpb::GRPCSinkOperator::descriptor()->FindFieldByName("grpc_source_id"));
    if (!differencer_.Compare(expected_node.pb.op(), actual_node.pb.op())) {
      return false;
    }
    return true;
  }

  bool CompareRoots(Graph* expected_graph, Graph* actual_graph, uint64_t expected_root,
                    uint64_t actual_root) const {
    std::queue<std::pair<uint64_t, uint64_t>> expected_actual_pairs;
    expected_actual_pairs.push({expected_root, actual_root});
    while (!expected_actual_pairs.empty()) {
      auto [expected, actual] = expected_actual_pairs.front();
      expected_actual_pairs.pop();
      auto expected_node = expected_graph->plan_map[expected];
      auto actual_node = actual_graph->plan_map[actual];
      if (!CompareNodes(expected_node, actual_node)) {
        return false;
      }
      // Children have an order to them, so if the exact pairing doesn't match we can say the
      // expected parent != actual parent.
      for (const auto& [i, expected_child] : Enumerate(expected_node.children)) {
        auto actual_child = actual_node.children[i];
        expected_actual_pairs.push({expected_child, actual_child});
      }
    }

    return true;
  }

  struct ExpectedToActual {
    absl::flat_hash_map<uint64_t, absl::flat_hash_set<uint64_t>> candidates;

    absl::flat_hash_map<uint64_t, uint64_t> matches;

    void AddMatch(const Graph& expected_graph, uint64_t expected, uint64_t actual) {
      for (const auto& expected_root : expected_graph.root_nodes) {
        candidates[expected_root].erase(actual);
      }
      matches[expected] = actual;
    }
  };

  bool MatchAndExplain(const planpb::Plan& pb, ::testing::MatchResultListener* listener) const {
    Graph actual_graph = BuildGraph(pb);
    Graph expected_graph = BuildGraph(expected_plan_);

    // Compare roots.
    if (actual_graph.root_nodes.size() != expected_graph.root_nodes.size()) {
      (*listener) << "different number of roots in the two graphs";
      (*listener) << ::px::testing::DiffLines(expected_plan_.DebugString(), pb.DebugString());
      return false;
    }

    ExpectedToActual expected_to_actual;
    absl::flat_hash_map<uint64_t, absl::flat_hash_set<uint64_t>> actual_to_expected_candidates;
    // Setup the empty sets.
    for (const auto& expected_root : expected_graph.root_nodes) {
      expected_to_actual.candidates[expected_root] = {};
    }
    for (const auto& actual_root : actual_graph.root_nodes) {
      actual_to_expected_candidates[actual_root] = {};
    }
    // Go through and compare the nodes and childrens with each other.
    for (const auto& actual_root : actual_graph.root_nodes) {
      for (const auto& expected_root : expected_graph.root_nodes) {
        if (CompareRoots(&expected_graph, &actual_graph, expected_root, actual_root)) {
          expected_to_actual.candidates[expected_root].insert(actual_root);
          actual_to_expected_candidates[actual_root].insert(expected_root);
        }
      }
    }
    for (const auto& [actual, expected_candidates] : actual_to_expected_candidates) {
      if (expected_candidates.empty()) {
        (*listener) << "No match for root actual node "
                    << actual_graph.plan_map[actual].DebugString();
        (*listener) << ::px::testing::DiffLines(expected_plan_.DebugString(), pb.DebugString());
        return false;
      }
    }

    // We iterate until we can map an expected root node for every actual root node
    // or we discover an unequal part of the graph.
    std::queue<uint64_t> expected_ids_q;
    for (const auto& [expected, _] : expected_to_actual.candidates) {
      expected_ids_q.push(expected);
    }

    while (!expected_ids_q.empty()) {
      uint64_t expected = expected_ids_q.front();
      expected_ids_q.pop();
      // Ignore the entries that already have matches.
      if (expected_to_actual.matches.contains(expected)) {
        continue;
      }
      auto actual_candidates = expected_to_actual.candidates[expected];
      if (actual_candidates.empty()) {
        (*listener) << "No match for root expected node "
                    << expected_graph.plan_map[expected].DebugString();
        (*listener) << ::px::testing::DiffLines(expected_plan_.DebugString(), pb.DebugString());
        return false;
      }

      if (actual_candidates.size() == 1) {
        uint64_t actual_candidate = *actual_candidates.begin();
        expected_to_actual.AddMatch(expected_graph, expected, actual_candidate);
        continue;
      }

      // There are conditions where the roots are interchangeable with one another among the
      // actual candidates. We use PARTIAL proto message differencer to determine comparable
      // plans, which means that an expected node might have several matches among the actual
      // nodes. At the same time, PARTIAL proto differencer is not commutative and therefore not
      // transitive, so we can't assume that if A -> 1, B -> 1, and B -> 2 that A -> 2 is also
      // true. See the tests for more details.
      //
      // The following tests the conditions
      // 1. Any of the actual candidates are the unique connection to an expected candidate.
      // 2. Any of the actual have more than 1 unique candidates - which means the graphs don't
      //    match
      // 3. If none of the candidates are unique, that means they are interchangeable.
      //    We just choose an order and roll with it.
      bool is_arbitrary = true;
      for (const auto& actual_i : actual_candidates) {
        // Compare all of the actual -> expected matches to this actual instance
        // to figure out if this one has a unique candidate for easy matches.
        auto unique_candidates = actual_to_expected_candidates[actual_i];
        for (const auto& actual_j : actual_candidates) {
          if (actual_i == actual_j) {
            continue;
          }
          for (const auto& c : actual_to_expected_candidates[actual_j]) {
            unique_candidates.erase(c);
          }
        }
        // This actual_i does not uniquely match any expected nodes.
        if (unique_candidates.empty()) {
          continue;
        }
        // This actual_i matches more than 1 expected value
        if (unique_candidates.size() != 1) {
          (*listener) << "Node matches too many expected nodes "
                      << actual_graph.plan_map[actual_i].DebugString();
          (*listener) << ::px::testing::DiffLines(expected_plan_.DebugString(), pb.DebugString());
          return false;
        }
        is_arbitrary = false;
        expected_to_actual.AddMatch(expected_graph, *unique_candidates.begin(), actual_i);
        break;
      }
      if (!is_arbitrary) {
        continue;
      }

      absl::flat_hash_set<uint64_t> actuals_to_match = actual_candidates;
      for (const auto& expected : actual_to_expected_candidates[*actual_candidates.begin()]) {
        uint64_t actual = *actuals_to_match.begin();
        actuals_to_match.erase(actual);
        expected_to_actual.AddMatch(expected_graph, expected, actual);
      }
    }

    return true;
  }

  virtual void DescribeTo(::std::ostream* os) const {
    *os << "equals to text probobuf: " << expected_plan_.DebugString();
  }

  virtual void DescribeNegationTo(::std::ostream* os) const {
    *os << "does not equal to text protobuf: " << expected_plan_.DebugString();
  }

  planpb::Plan expected_plan_;
};

inline ::testing::PolymorphicMatcher<PlanGraphMatcher> EqualsPlanGraph(planpb::Plan plan) {
  return ::testing::MakePolymorphicMatcher(PlanGraphMatcher(std::move(plan)));
}

inline ::testing::PolymorphicMatcher<PlanGraphMatcher> EqualsPlanGraph(
    const std::string& plan_txt) {
  planpb::Plan expected_plan;
  google::protobuf::TextFormat::MergeFromString(plan_txt, &expected_plan);
  return ::testing::MakePolymorphicMatcher(PlanGraphMatcher(std::move(expected_plan)));
}

}  // namespace testing
}  // namespace planner
}  // namespace carnot
}  // namespace px
