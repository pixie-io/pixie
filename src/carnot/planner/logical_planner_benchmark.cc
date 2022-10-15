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

#include <benchmark/benchmark.h>

#include "src/carnot/planner/logical_planner.h"
#include "src/carnot/planner/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/perf/perf.h"
#include "src/common/testing/testing.h"

namespace px {
namespace carnot {
namespace planner {
namespace logical_planner {

// NOLINTNEXTLINE : runtime/references.
void BM_Query(benchmark::State& state) {
  auto info = udfexporter::ExportUDFInfo().ConsumeValueOrDie()->info_pb();
  auto planner = LogicalPlanner::Create(info).ConsumeValueOrDie();
  plannerpb::QueryRequest query_request;
  query_request.set_query_str(testutils::kHttpRequestStats);
  *query_request.mutable_logical_planner_state() =
      testutils::CreateTwoPEMsOneKelvinPlannerState(testutils::kHttpEventsSchema);
  for (auto _ : state) {
    auto plan_or_s = planner->Plan(query_request);
    EXPECT_OK(plan_or_s);
  }
}

BENCHMARK(BM_Query);

}  // namespace logical_planner
}  // namespace planner
}  // namespace carnot
}  // namespace px
