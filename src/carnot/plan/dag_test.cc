#include "src/carnot/plan/dag.h"
#include <gtest/gtest.h>
#include <unordered_set>

namespace pl {
namespace carnot {
namespace plan {

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
  EXPECT_EQ(std::unordered_set<int>({5, 8, 3, 6, 20}), dag_.nodes());
  EXPECT_EQ(std::vector<int>({8, 3}), dag_.DependenciesOf(5));
  EXPECT_EQ(std::vector<int>({}), dag_.DependenciesOf(1));
  EXPECT_TRUE(dag_.HasNode(5));
  EXPECT_FALSE(dag_.HasNode(36));
}

TEST_F(DAGTest, check_delete) {
  dag_.DeleteEdge(5, 8);
  EXPECT_EQ(std::vector<int>({3}), dag_.DependenciesOf(5));
}

TEST_F(DAGTest, orphans) { EXPECT_EQ(std::unordered_set<int>({20}), dag_.Orphans()); }

TEST_F(DAGTest, delete_node) {
  dag_.DeleteNode(8);
  EXPECT_EQ(std::vector<int>({}), dag_.DependenciesOf(8));
  EXPECT_EQ(std::vector<int>({3}), dag_.DependenciesOf(5));
}
TEST_F(DAGTest, check_delete_add) {
  dag_.DeleteNode(8);
  EXPECT_FALSE(dag_.HasNode(8));
  dag_.AddNode(8);
  EXPECT_TRUE(dag_.HasNode(8));
}

TEST_F(DAGTest, transitive_deps) {
  EXPECT_EQ(std::unordered_set<int>({8, 3, 6}), dag_.TransitiveDepsFrom(5));
  EXPECT_EQ(std::unordered_set<int>({6}), dag_.TransitiveDepsFrom(3));
  EXPECT_EQ(std::unordered_set<int>({}), dag_.TransitiveDepsFrom(6));
}

TEST_F(DAGTest, topological_sort) {
  EXPECT_EQ(std::vector<int>({20, 5, 8, 3, 6}), dag_.TopologicalSort());

  dag_.DeleteNode(20);
  EXPECT_EQ(std::vector<int>({5, 8, 3, 6}), dag_.TopologicalSort());

  dag_.DeleteNode(8);
  EXPECT_EQ(std::vector<int>({5, 3, 6}), dag_.TopologicalSort());
}

using DAGDeathTest = DAGTest;
TEST_F(DAGDeathTest, check_add_duplicate) { EXPECT_DEBUG_DEATH(dag_.AddNode(5), ".*"); }

TEST_F(DAGDeathTest, check_failure_on_cycle) {
  dag_.AddEdge(6, 5);
  EXPECT_DEATH(dag_.TopologicalSort(), ".*Cycle.*");
  EXPECT_DEATH(dag_.TransitiveDepsFrom(5), ".*Cycle.*");
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
