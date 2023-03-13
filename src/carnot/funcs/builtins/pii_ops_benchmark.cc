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

#include "src/carnot/funcs/builtins/pii_ops.h"

namespace px {
namespace carnot {
namespace builtins {

static constexpr std::string_view input_chunk = R"input(
        255.255.255.255
        00ab:1234:5678:9100:1112:ffff:192.168.0.1
        "valid_ip_in_email255.255.255.255@mydomain.com",
        "00:00:00:00:00:00",
        "3530 1113333 00000",  "3566 0020203 60505", "5555 5555555 54444", "5105 1051051 05100",
        "52-635024-498175-97", "51-942642-588642-2",  "51-942642-588642-55",
        "UA21 3223 1300 0002 6007 2335 6600 1",
        "201-21-0021", "211-11-2011",
)input";

// NOLINTNEXTLINE : runtime/references.
static void BM_RedactPII(benchmark::State& state) {
  RedactPIIUDF udf;
  PX_UNUSED(udf.Init(nullptr));

  std::string text_chunk(input_chunk);
  std::string text;
  for (int i = 0; i < state.range(0); i++) {
    text += text_chunk;
  }
  for (auto _ : state) {
    benchmark::DoNotOptimize(udf.Exec(nullptr, text));
  }
  state.SetBytesProcessed(static_cast<int64_t>(text.length()) *
                          static_cast<int64_t>(state.iterations()));
}

BENCHMARK(BM_RedactPII)->RangeMultiplier(2)->Range(1, 12);

}  // namespace builtins
}  // namespace carnot
}  // namespace px
