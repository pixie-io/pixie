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

#include <gtest/gtest.h>

#include "src/carnot/planner/ir/column_expression.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/join_ir.h"
#include "src/carnot/planner/ir/map_ir.h"
#include "src/carnot/planner/ir/memory_sink_ir.h"
#include "src/carnot/planner/ir/memory_source_ir.h"
#include "src/common/testing/testing.h"

using ::testing::UnorderedElementsAre;

namespace px {
namespace carnot {
namespace planner {

TEST(IndependentGraphs, simple_map) {
  IR ir;
  // First connected component is a simple map:
  // MemSrc -> Map -> MemSink
  auto src1 = ir.CreateNode<MemorySourceIR>(nullptr, "table", std::vector<std::string>{})
                  .ConsumeValueOrDie();
  auto map1 = ir.CreateNode<MapIR>(nullptr, src1, ColExpressionVector{}, true).ConsumeValueOrDie();
  auto sink1 = ir.CreateNode<MemorySinkIR>(nullptr, map1, "output", std::vector<std::string>{})
                   .ConsumeValueOrDie();

  // Second connected component has 2 maps.
  auto src2 = ir.CreateNode<MemorySourceIR>(nullptr, "table", std::vector<std::string>{})
                  .ConsumeValueOrDie();
  auto map2 = ir.CreateNode<MapIR>(nullptr, src2, ColExpressionVector{}, true).ConsumeValueOrDie();
  auto map3 = ir.CreateNode<MapIR>(nullptr, map2, ColExpressionVector{}, true).ConsumeValueOrDie();
  auto sink2 = ir.CreateNode<MemorySinkIR>(nullptr, map3, "output", std::vector<std::string>{})
                   .ConsumeValueOrDie();

  EXPECT_THAT(
      ir.IndependentGraphs(),
      UnorderedElementsAre(UnorderedElementsAre(src1->id(), map1->id(), sink1->id()),
                           UnorderedElementsAre(src2->id(), map2->id(), map3->id(), sink2->id())));
}

TEST(IndependentGraphs, simple_join) {
  IR ir;
  // First connected component is a simple map:
  // MemSrc -> Map -> MemSink
  auto src1 = ir.CreateNode<MemorySourceIR>(nullptr, "table", std::vector<std::string>{})
                  .ConsumeValueOrDie();
  auto map1 = ir.CreateNode<MapIR>(nullptr, src1, ColExpressionVector{}, true).ConsumeValueOrDie();
  auto sink1 = ir.CreateNode<MemorySinkIR>(nullptr, map1, "output", std::vector<std::string>{})
                   .ConsumeValueOrDie();

  // Second connected component has a join between 2 memory sources.
  auto src2 = ir.CreateNode<MemorySourceIR>(nullptr, "table", std::vector<std::string>{})
                  .ConsumeValueOrDie();
  auto src3 = ir.CreateNode<MemorySourceIR>(nullptr, "table", std::vector<std::string>{})
                  .ConsumeValueOrDie();
  auto join = ir.CreateNode<JoinIR>(nullptr, std::vector<OperatorIR*>{src2, src3}, "inner",
                                    std::vector<ColumnIR*>{}, std::vector<ColumnIR*>{},
                                    std::vector<std::string>{"left", "right"})
                  .ConsumeValueOrDie();
  auto sink2 = ir.CreateNode<MemorySinkIR>(nullptr, join, "output", std::vector<std::string>{})
                   .ConsumeValueOrDie();

  EXPECT_THAT(
      ir.IndependentGraphs(),
      UnorderedElementsAre(UnorderedElementsAre(src1->id(), map1->id(), sink1->id()),
                           UnorderedElementsAre(src2->id(), src3->id(), join->id(), sink2->id())));
}

TEST(IndependentGraphs, complex_3_src_join_bug) {
  IR ir;
  // First connected component is a simple map:
  // MemSrc -> Map -> MemSink
  auto src1 = ir.CreateNode<MemorySourceIR>(nullptr, "table", std::vector<std::string>{})
                  .ConsumeValueOrDie();
  auto map1 = ir.CreateNode<MapIR>(nullptr, src1, ColExpressionVector{}, true).ConsumeValueOrDie();
  auto sink1 = ir.CreateNode<MemorySinkIR>(nullptr, map1, "output", std::vector<std::string>{})
                   .ConsumeValueOrDie();

  // Second component is a join between 3 sources with 3 joins. This input graph lead to a
  // non-deterministic bug (based on the order the sources were processed) in a previous
  // implementation of IndependentGraphs.
  // The graph looks as follows:
  //                 Src2           Src3
  //            /           \       /
  //           /             \     /
  //         Map      Src4     Join2
  //          |    /      \     |
  //          |   /         \   |
  //          Join1           Join3
  //           |                |
  //          Sink            Sink
  auto src2 = ir.CreateNode<MemorySourceIR>(nullptr, "table", std::vector<std::string>{})
                  .ConsumeValueOrDie();
  auto src3 = ir.CreateNode<MemorySourceIR>(nullptr, "table", std::vector<std::string>{})
                  .ConsumeValueOrDie();
  auto src4 = ir.CreateNode<MemorySourceIR>(nullptr, "table", std::vector<std::string>{})
                  .ConsumeValueOrDie();

  auto map2 = ir.CreateNode<MapIR>(nullptr, src2, ColExpressionVector{}, true).ConsumeValueOrDie();
  auto join1 = ir.CreateNode<JoinIR>(nullptr, std::vector<OperatorIR*>{map2, src4}, "inner",
                                     std::vector<ColumnIR*>{}, std::vector<ColumnIR*>{},
                                     std::vector<std::string>{"left", "right"})
                   .ConsumeValueOrDie();
  auto sink2 = ir.CreateNode<MemorySinkIR>(nullptr, join1, "output", std::vector<std::string>{})
                   .ConsumeValueOrDie();

  auto join2 = ir.CreateNode<JoinIR>(nullptr, std::vector<OperatorIR*>{src2, src3}, "inner",
                                     std::vector<ColumnIR*>{}, std::vector<ColumnIR*>{},
                                     std::vector<std::string>{"left", "right"})
                   .ConsumeValueOrDie();
  auto join3 = ir.CreateNode<JoinIR>(nullptr, std::vector<OperatorIR*>{src4, join2}, "inner",
                                     std::vector<ColumnIR*>{}, std::vector<ColumnIR*>{},
                                     std::vector<std::string>{"left", "right"})
                   .ConsumeValueOrDie();
  auto sink3 = ir.CreateNode<MemorySinkIR>(nullptr, join3, "output", std::vector<std::string>{})
                   .ConsumeValueOrDie();

  EXPECT_THAT(ir.IndependentGraphs(),
              UnorderedElementsAre(
                  UnorderedElementsAre(src1->id(), map1->id(), sink1->id()),
                  UnorderedElementsAre(src2->id(), src3->id(), src4->id(), map2->id(), join1->id(),
                                       join2->id(), join3->id(), sink2->id(), sink3->id())));
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
