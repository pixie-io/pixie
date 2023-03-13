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

#include <picohttpparser.h>

#include <random>

#include <benchmark/benchmark.h>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http/body_decoder.h"

using px::stirling::protocols::http::ParseChunked;

const size_t kBodyLimitSizeBytes = 1000000;

std::string CreateData(size_t chunks) {
  std::string s;

  std::default_random_engine rng(37);
  std::uniform_int_distribution<int> uniform_dist(1, 1000);

  for (size_t i = 0; i < chunks; ++i) {
    size_t chunk_size = uniform_dist(rng);

    std::stringstream sstream;
    sstream << std::hex << chunk_size;

    s += sstream.str();
    size_t s1 = s.size();
    s += px::CreateStringView<char>("\r\n");
    DCHECK_EQ(s1 + 2, s.size());
    s += std::string(chunk_size, 'x');
    s += px::CreateStringView<char>("\r\n");
  }

  s += px::CreateStringView<char>("0\r\n");
  s += px::CreateStringView<char>("\r\n");

  return s;
}

// NOLINTNEXTLINE: runtime/string
const std::string data = CreateData(1000);

// NOLINTNEXTLINE(runtime/references)
static void BM_custom_body_parser(benchmark::State& state) {
  FLAGS_use_pico_chunked_decoder = false;

  for (auto _ : state) {
    std::string result;
    size_t body_size;
    std::string_view data_view(data);
    px::stirling::ParseState parse_state = px::stirling::protocols::http::ParseChunked(
        &data_view, kBodyLimitSizeBytes, &result, &body_size);
    CHECK(parse_state == px::stirling::ParseState::kSuccess);
    benchmark::DoNotOptimize(parse_state);
    benchmark::DoNotOptimize(result);
  }
}

// NOLINTNEXTLINE(runtime/references)
static void BM_pico_body_parser(benchmark::State& state) {
  FLAGS_use_pico_chunked_decoder = true;

  for (auto _ : state) {
    std::string result;
    size_t body_size;
    std::string_view data_view(data);

    px::stirling::ParseState parse_state =
        ParseChunked(&data_view, kBodyLimitSizeBytes, &result, &body_size);
    CHECK(parse_state == px::stirling::ParseState::kSuccess);
    benchmark::DoNotOptimize(parse_state);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(BM_custom_body_parser);
BENCHMARK(BM_pico_body_parser);
