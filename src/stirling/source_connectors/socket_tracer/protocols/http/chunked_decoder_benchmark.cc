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
 * SPDX-License-Identifier: Apache-2.0
 */

#include <picohttpparser.h>

#include <random>

#include <benchmark/benchmark.h>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http/chunked_decoder.h"

using px::stirling::protocols::http::ParseChunked;

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
    std::string_view data_view(data);
    px::stirling::ParseState parse_state =
        px::stirling::protocols::http::ParseChunked(&data_view, &result);
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
    std::string_view data_view(data);

    px::stirling::ParseState parse_state = ParseChunked(&data_view, &result);
    CHECK(parse_state == px::stirling::ParseState::kSuccess);
    benchmark::DoNotOptimize(parse_state);
    benchmark::DoNotOptimize(result);
  }
}

BENCHMARK(BM_custom_body_parser);
BENCHMARK(BM_pico_body_parser);
