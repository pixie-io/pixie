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

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "src/carnot/planner/compiler/analyzer/drop_to_map_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using table_store::schema::Relation;
using ::testing::_;
using ::testing::ElementsAre;

TEST_F(RulesTest, drop_to_map) {
  MemorySourceIR* mem_src =
      graph->CreateNode<MemorySourceIR>(ast, "source", std::vector<std::string>{})
          .ConsumeValueOrDie();
  DropIR* drop = graph->CreateNode<DropIR>(ast, mem_src, std::vector<std::string>{"cpu0", "cpu1"})
                     .ConsumeValueOrDie();
  MemorySinkIR* sink = MakeMemSink(drop, "sink");

  EXPECT_OK(mem_src->SetRelation(cpu_relation));
  EXPECT_THAT(graph->dag().TopologicalSort(), ElementsAre(0, 1, 2));

  auto drop_id = drop->id();

  // Apply the rule.
  DropToMapOperatorRule rule(compiler_state_.get());
  auto status = rule.Execute(graph.get());
  ASSERT_OK(status);
  EXPECT_TRUE(status.ValueOrDie());

  EXPECT_FALSE(graph->dag().HasNode(drop_id));

  ASSERT_EQ(mem_src->Children().size(), 1);
  EXPECT_MATCH(mem_src->Children()[0], Map());
  auto op = static_cast<MapIR*>(mem_src->Children()[0]);
  EXPECT_EQ(op->col_exprs().size(), 2);
  EXPECT_EQ(op->col_exprs()[0].name, "count");
  EXPECT_EQ(op->col_exprs()[1].name, "cpu2");

  EXPECT_TRUE(Match(op->col_exprs()[0].node, ColumnNode("count")))
      << op->col_exprs()[0].node->DebugString();
  EXPECT_TRUE(Match(op->col_exprs()[1].node, ColumnNode("cpu2")))
      << op->col_exprs()[1].node->DebugString();

  EXPECT_EQ(op->relation(), Relation({types::INT64, types::FLOAT64}, {"count", "cpu2"}));

  EXPECT_EQ(op->Children().size(), 1);
  EXPECT_EQ(op->Children()[0], sink);
}

TEST_F(RulesTest, drop_middle_columns) {
  MemorySourceIR* mem_src =
      MakeMemSource(Relation({types::STRING, types::TIME64NS, types::STRING, types::FLOAT64,
                              types::FLOAT64, types::TIME64NS},
                             {"service", "window", "quantiles", "p50", "p99", "time_"}));
  DropIR* drop =
      graph->CreateNode<DropIR>(ast, mem_src, std::vector<std::string>{"window", "quantiles"})
          .ConsumeValueOrDie();
  auto drop_id = drop->id();
  MemorySinkIR* sink = MakeMemSink(drop, "sink");

  EXPECT_THAT(graph->dag().TopologicalSort(), ElementsAre(0, 1, 2));

  // Apply the rule.
  DropToMapOperatorRule rule(compiler_state_.get());
  auto status = rule.Execute(graph.get());
  ASSERT_OK(status);
  EXPECT_TRUE(status.ValueOrDie());

  EXPECT_FALSE(graph->dag().HasNode(drop_id));

  ASSERT_EQ(mem_src->Children().size(), 1);
  EXPECT_MATCH(mem_src->Children()[0], Map());
  auto op = static_cast<MapIR*>(mem_src->Children()[0]);
  EXPECT_EQ(op->col_exprs().size(), 4);
  EXPECT_EQ(op->col_exprs()[0].name, "service");
  EXPECT_EQ(op->col_exprs()[1].name, "p50");
  EXPECT_EQ(op->col_exprs()[2].name, "p99");
  EXPECT_EQ(op->col_exprs()[3].name, "time_");

  EXPECT_TRUE(Match(op->col_exprs()[0].node, ColumnNode("service")))
      << op->col_exprs()[0].node->DebugString();
  EXPECT_TRUE(Match(op->col_exprs()[1].node, ColumnNode("p50")))
      << op->col_exprs()[1].node->DebugString();
  EXPECT_TRUE(Match(op->col_exprs()[2].node, ColumnNode("p99")))
      << op->col_exprs()[2].node->DebugString();
  EXPECT_TRUE(Match(op->col_exprs()[3].node, ColumnNode("time_")))
      << op->col_exprs()[3].node->DebugString();

  EXPECT_EQ(op->relation(),
            Relation({types::STRING, types::FLOAT64, types::FLOAT64, types::TIME64NS},
                     {"service", "p50", "p99", "time_"}));
  EXPECT_EQ(op->Children().size(), 1);
  EXPECT_EQ(op->Children()[0], sink);
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
