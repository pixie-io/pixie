/*
 * Copyright © 2018- Pixie Labs Inc.
 * Copyright © 2020- New Relic, Inc.
 * All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of New Relic Inc. and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Pixie Labs Inc. and its suppliers and
 * may be covered by U.S. and Foreign Patents, patents in process,
 * and are protected by trade secret or copyright law. Dissemination
 * of this information or reproduction of this material is strictly
 * forbidden unless prior written permission is obtained from
 * New Relic, Inc.
 *
 * SPDX-License-Identifier: Proprietary
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
