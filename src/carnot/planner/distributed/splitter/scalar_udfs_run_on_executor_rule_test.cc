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
#include "src/carnot/planner/distributed/splitter/scalar_udfs_run_on_executor_rule.h"
#include "src/carnot/planner/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace distributed {

using compiler::ResolveTypesRule;
using testutils::DistributedRulesTest;

TEST_F(DistributedRulesTest, ScalarUDFRunOnKelvinRuleTest) {
  // Kelvin-only plan
  MemorySourceIR* src1 = MakeMemSource("http_events");
  auto func1 = MakeFunc("kelvin_only", {});
  MapIR* map1 = MakeMap(src1, {{"out", func1}});
  MakeMemSink(map1, "foo", {});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  ScalarUDFsRunOnKelvinRule rule(compiler_state_.get());

  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_FALSE(rule_or_s.ConsumeValueOrDie());

  // PEM-only plan
  MemorySourceIR* src2 = MakeMemSource("http_events");
  auto func2 = MakeFunc("pem_only", {});
  MapIR* map2 = MakeMap(src2, {{"out", func2}}, false);
  MakeMemSink(map2, "foo", {});

  ResolveTypesRule type_rule2(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  rule_or_s = rule.Execute(graph.get());
  ASSERT_NOT_OK(rule_or_s);
  EXPECT_THAT(
      rule_or_s.status(),
      HasCompilerError(
          "UDF 'pem_only' must execute before blocking nodes such as limit, agg, and join."));
}

TEST_F(DistributedRulesTest, ScalarUDFRunOnPEMRuleTest) {
  // PEM-only plan
  MemorySourceIR* src1 = MakeMemSource("http_events");
  auto func1 = MakeFunc("pem_only", {});
  auto equals_func1 = MakeEqualsFunc(func1, MakeString("abc"));
  FilterIR* filter1 = MakeFilter(src1, equals_func1);
  MakeMemSink(filter1, "foo", {});

  ResolveTypesRule type_rule(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  ScalarUDFsRunOnPEMRule rule(compiler_state_.get());

  auto rule_or_s = rule.Execute(graph.get());
  ASSERT_OK(rule_or_s);
  ASSERT_FALSE(rule_or_s.ConsumeValueOrDie());

  // Kelvin-only plan
  MemorySourceIR* src2 = MakeMemSource("http_events");
  auto func2 = MakeFunc("kelvin_only", {});
  auto equals_func2 = MakeEqualsFunc(func2, MakeString("abc"));
  FilterIR* filter2 = MakeFilter(src2, equals_func2);
  MakeMemSink(filter2, "foo", {});

  ResolveTypesRule type_rule2(compiler_state_.get());
  ASSERT_OK(type_rule.Execute(graph.get()));

  rule_or_s = rule.Execute(graph.get());
  ASSERT_NOT_OK(rule_or_s);
  EXPECT_THAT(
      rule_or_s.status(),
      HasCompilerError(
          "UDF 'kelvin_only' must execute after blocking nodes such as limit, agg, and join."));
}

}  // namespace distributed
}  // namespace planner
}  // namespace carnot
}  // namespace px
