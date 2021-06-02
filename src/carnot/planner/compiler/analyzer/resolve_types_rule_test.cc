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

#include "src/carnot/planner/compiler/analyzer/resolve_types_rule.h"
#include "src/carnot/planner/compiler/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using ResolveTypesRuleTest = RulesTest;

TEST_F(ResolveTypesRuleTest, map_then_agg) {
  auto mem_src = MakeMemSource("semantic_table", {"cpu", "bytes"});
  auto map = MakeMap(
      mem_src,
      {
          ColumnExpression("cpu_sum", MakeAddFunc(MakeColumn("cpu", 0), MakeColumn("cpu", 0))),
          ColumnExpression("bytes_sum",
                           MakeAddFunc(MakeColumn("bytes", 0), MakeColumn("bytes", 0))),
      });
  auto agg = MakeBlockingAgg(
      map, /* groups */ {},
      {
          ColumnExpression("cpu_sum_mean", MakeMeanFunc(MakeColumn("cpu_sum", 0))),
          ColumnExpression("bytes_sum_mean", MakeMeanFunc(MakeColumn("bytes_sum", 0))),
      });

  ResolveTypesRule types_rule(compiler_state_.get());
  auto result = types_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_TRUE(result.ValueOrDie());

  auto map_type = std::static_pointer_cast<TableType>(map->resolved_type());
  auto agg_type = std::static_pointer_cast<TableType>(agg->resolved_type());

  // Add gets rid of ST_PERCENT but not ST_BYTES
  EXPECT_TableHasColumnWithType(map_type, "cpu_sum",
                                ValueType::Create(types::FLOAT64, types::ST_NONE))
      EXPECT_TableHasColumnWithType(map_type, "bytes_sum",
                                    ValueType::Create(types::INT64, types::ST_BYTES));
  EXPECT_TableHasColumnWithType(agg_type, "cpu_sum_mean",
                                ValueType::Create(types::FLOAT64, types::ST_NONE));
  // Note that mean turns Int->Float.
  EXPECT_TableHasColumnWithType(agg_type, "bytes_sum_mean",
                                ValueType::Create(types::FLOAT64, types::ST_BYTES));

  // The types rule shouldn't change anything after the first pass.
  result = types_rule.Execute(graph.get());
  ASSERT_OK(result);
  EXPECT_FALSE(result.ValueOrDie());
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
