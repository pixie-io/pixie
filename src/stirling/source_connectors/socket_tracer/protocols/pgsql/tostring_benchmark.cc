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

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/pgsql/types.h"

using ::px::stirling::protocols::pgsql::FmtCode;
using ::px::stirling::protocols::pgsql::RowDesc;

// NOLINTNEXTLINE(runtime/references)
static void BM_row_desc_tostring(benchmark::State& state) {
  RowDesc::Field field = {"id", 18427, 1, 2950, 16, -1, FmtCode::kText};
  RowDesc row_desc;
  for (int i = 0; i < state.range(0); ++i) {
    row_desc.fields.push_back(field);
  }

  for (auto _ : state) {
    std::string out = row_desc.ToString();
    benchmark::DoNotOptimize(out);
  }
}

BENCHMARK(BM_row_desc_tostring)->DenseRange(1, 10, 1);
