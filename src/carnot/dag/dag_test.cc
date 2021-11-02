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

#include "src/carnot/dag/dag.h"

#include <algorithm>
#include <memory>
#include <unordered_set>

#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include "src/common/testing/protobuf.h"

namespace px {
namespace carnot {
namespace plan {

using ::px::testing::proto::EqualsProto;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

class DAGTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dag_.AddNode(5);
    dag_.AddNode(8);
    dag_.AddNode(3);
    dag_.AddNode(6);
    dag_.AddNode(20);

    dag_.AddEdge(5, 8);
    dag_.AddEdge(5, 3);
    dag_.AddEdge(8, 3);
    dag_.AddEdge(3, 6);
  }
  DAG dag_;
};

TEST_F(DAGTest, basic_test) {
  EXPECT_THAT(dag_.nodes(), UnorderedElementsAre(5, 8, 3, 6, 20));
  EXPECT_THAT(dag_.DependenciesOf(5), ElementsAre(8, 3));
  EXPECT_THAT(dag_.DependenciesOf(1), ElementsAre());
  EXPECT_TRUE(dag_.HasNode(5));
  EXPECT_FALSE(dag_.HasNode(36));
}

TEST_F(DAGTest, check_delete) {
  dag_.DeleteEdge(5, 8);
  EXPECT_THAT(dag_.DependenciesOf(5), ElementsAre(3));
}

TEST_F(DAGTest, orphans) { EXPECT_THAT(dag_.Orphans(), UnorderedElementsAre(20)); }

TEST_F(DAGTest, delete_node) {
  dag_.DeleteNode(8);
  EXPECT_THAT(dag_.DependenciesOf(8), ElementsAre());
  EXPECT_THAT(dag_.ParentsOf(8), ElementsAre());
  EXPECT_THAT(dag_.DependenciesOf(5), ElementsAre(3));
}

TEST_F(DAGTest, check_delete_add) {
  dag_.DeleteNode(8);
  EXPECT_FALSE(dag_.HasNode(8));
  dag_.AddNode(8);
  EXPECT_TRUE(dag_.HasNode(8));
}

TEST_F(DAGTest, transitive_deps) {
  EXPECT_THAT(dag_.TransitiveDepsFrom(5), UnorderedElementsAre(8, 3, 6));
  EXPECT_THAT(dag_.TransitiveDepsFrom(3), UnorderedElementsAre(6));
  EXPECT_THAT(dag_.TransitiveDepsFrom(6), UnorderedElementsAre());
}

TEST_F(DAGTest, topological_sort) {
  EXPECT_THAT(dag_.TopologicalSort(),
              AnyOf(ElementsAre(20, 5, 8, 3, 6), ElementsAre(5, 20, 8, 3, 6)));

  dag_.DeleteNode(20);
  EXPECT_THAT(dag_.TopologicalSort(), ElementsAre(5, 8, 3, 6));

  dag_.DeleteNode(8);
  EXPECT_THAT(dag_.TopologicalSort(), ElementsAre(5, 3, 6));
}

using DAGDeathTest = DAGTest;
TEST_F(DAGDeathTest, check_add_duplicate) { EXPECT_DEBUG_DEATH(dag_.AddNode(5), ".*"); }

TEST_F(DAGDeathTest, check_failure_on_cycle) {
  dag_.AddEdge(6, 5);
  EXPECT_DEATH(dag_.TopologicalSort(), ".*Cycle.*");
  EXPECT_DEATH(dag_.TransitiveDepsFrom(5), ".*Cycle.*");
}

/**
 * @brief Creates three separate graphs within the DAG.
 */
class DAGTestMultipleSubGraphs : public ::testing::Test {
 protected:
  void SetUp() override {
    dag_.AddNode(1);
    dag_.AddNode(2);
    dag_.AddNode(3);
    dag_.AddNode(4);
    dag_.AddNode(5);
    dag_.AddNode(6);
    dag_.AddNode(7);
    dag_.AddNode(8);
    dag_.AddNode(9);
    dag_.AddNode(10);
    dag_.AddNode(11);
    dag_.AddNode(12);
    dag_.AddNode(13);

    // #1 has two sources and 1 sink.
    dag_.AddEdge(1, 2);
    dag_.AddEdge(4, 5);
    dag_.AddEdge(5, 2);
    dag_.AddEdge(2, 3);

    // #2 has 1 source and 1 sink, is linear.
    dag_.AddEdge(6, 7);
    dag_.AddEdge(7, 8);

    // #3 has 1 source and 2 sinks.
    dag_.AddEdge(9, 10);
    dag_.AddEdge(10, 11);
    dag_.AddEdge(10, 12);
    dag_.AddEdge(12, 13);
  }
  DAG dag_;
};

TEST_F(DAGTestMultipleSubGraphs, delete_node_removes_all_deps) {
  // When there were two elements as children, this used to fail.
  dag_.AddEdge(10, 13);
  EXPECT_EQ(dag_.DependenciesOf(10).size(), 3);
  EXPECT_EQ(dag_.ParentsOf(10).size(), 1);
  dag_.DeleteNode(10);
  EXPECT_EQ(dag_.DependenciesOf(10).size(), 0);

  EXPECT_EQ(dag_.ParentsOf(10).size(), 0);
  EXPECT_EQ(dag_.ParentsOf(11).size(), 0);
  EXPECT_EQ(dag_.ParentsOf(12).size(), 0);
  EXPECT_EQ(dag_.ParentsOf(13).size(), 1);
}

TEST_F(DAGTest, replace_child_node_edges_test) {
  // replace edges should preserve the order of the original edges in the DAG.
  EXPECT_THAT(dag_.DependenciesOf(5), ElementsAre(8, 3));
  EXPECT_THAT(dag_.ParentsOf(6), ElementsAre(3));
  EXPECT_THAT(dag_.ParentsOf(8), ElementsAre(5));
  dag_.ReplaceChildEdge(/* parent_node */ 5, /* old_child_node */ 8, /* new_child_node */ 6);
  EXPECT_THAT(dag_.DependenciesOf(5), ElementsAre(6, 3));
  EXPECT_THAT(dag_.ParentsOf(6), ElementsAre(3, 5));
  EXPECT_THAT(dag_.ParentsOf(8), ElementsAre());
}

TEST_F(DAGTest, replace_parent_node_edges_test) {
  // Replace parent node should preserve the order of the edges.
  EXPECT_THAT(dag_.ParentsOf(3), ElementsAre(5, 8));
  EXPECT_THAT(dag_.DependenciesOf(20), ElementsAre());
  EXPECT_THAT(dag_.DependenciesOf(5), ElementsAre(8, 3));

  dag_.ReplaceParentEdge(/* child_node */ 3, /* old_parent_node */ 5, /* new_parent_node */ 20);
  EXPECT_THAT(dag_.DependenciesOf(5), ElementsAre(8));
  EXPECT_THAT(dag_.ParentsOf(3), ElementsAre(20, 8));
  EXPECT_THAT(dag_.DependenciesOf(20), ElementsAre(3));
}

constexpr char kDAGProto[] = R"proto(
nodes {
  id: 5
  sorted_children: 8
  sorted_children: 3
}
nodes {
  id: 8
  sorted_parents: 5
  sorted_children: 3
}
nodes {
  id: 20
}
nodes {
  id: 3
  sorted_parents: 5
  sorted_parents: 8
  sorted_children: 6
}
nodes {
  id: 6
  sorted_parents: 3
}

)proto";

TEST_F(DAGTest, to_proto) {
  planpb::DAG pb;
  dag_.ToProto(&pb);
  google::protobuf::util::MessageDifferencer differencer;
  differencer.TreatAsSet(pb.GetDescriptor()->FindFieldByName("nodes"));
  EXPECT_THAT(pb, WithDifferencer(&differencer, EqualsProto(kDAGProto)));
}

constexpr char kDAGProtoIgnoreIds[] = R"proto(
nodes {
  id: 5
  sorted_children: 8
  sorted_children: 3
}
nodes {
  id: 8
  sorted_parents: 5
  sorted_children: 3
}
nodes {
  id: 3
  sorted_parents: 5
  sorted_parents: 8
}

)proto";
TEST_F(DAGTest, to_proto_ignore_ids) {
  planpb::DAG pb;
  dag_.ToProto(&pb, {6, 20});
  EXPECT_THAT(pb, EqualsProto(kDAGProtoIgnoreIds));
}

TEST_F(DAGTest, from_proto) {
  DAG new_dag;
  planpb::DAG pb;

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kDAGProto, &pb));
  new_dag.Init(pb);
  EXPECT_THAT(dag_.nodes(), UnorderedElementsAre(5, 8, 3, 6, 20));
  // Children should be ordered.
  EXPECT_THAT(dag_.DependenciesOf(5), ElementsAre(8, 3));
  // Parents should be ordered.
  EXPECT_THAT(dag_.ParentsOf(3), ElementsAre(5, 8));

  EXPECT_TRUE(dag_.DependenciesOf(1).empty());

  EXPECT_TRUE(dag_.HasNode(5));
  EXPECT_FALSE(dag_.HasNode(36));

  EXPECT_THAT(dag_.TopologicalSort(),
              AnyOf(ElementsAre(20, 5, 8, 3, 6), ElementsAre(5, 20, 8, 3, 6)));
}

}  // namespace plan
}  // namespace carnot
}  // namespace px
