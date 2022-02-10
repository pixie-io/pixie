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

#include <random>

#include "src/common/base/base.h"

#include "src/stirling/source_connectors/socket_tracer/protocols/common/always_contiguous_data_stream_buffer_impl.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/lazy_contiguous_data_stream_buffer_impl.h"

template <typename TDataStreamBufferImpl>
// NOLINTNEXTLINE : runtime/references.
static void BM_ContiguousBytes(benchmark::State& state) {
  size_t capacity = 50 * 1024 * 1024;
  size_t max_gap_size = 10 * 1024 * 1024;
  size_t allow_before_gap_size = 1 * 1024 * 1024;

  std::string data(state.range(0), '0');

  int n_chunks = capacity / data.size();

  for (auto _ : state) {
    state.PauseTiming();
    TDataStreamBufferImpl stream_buffer(capacity, max_gap_size, allow_before_gap_size);
    state.ResumeTiming();

    size_t pos = 0;
    uint64_t ts = 0;
    // Add a full capacity worth of contiguous bytes.
    for (int i = 0; i < n_chunks; ++i) {
      stream_buffer.Add(pos, data, ts);
      pos += data.size();
      ts += 1;
    }

    // Access head.
    benchmark::DoNotOptimize(stream_buffer.Head());
  }
  state.SetBytesProcessed(static_cast<uint64_t>(state.iterations()) * n_chunks * data.size());
}

template <typename TDataStreamBufferImpl>
// NOLINTNEXTLINE : runtime/references.
static void BM_SingleAdd(benchmark::State& state) {
  size_t capacity = 50 * 1024 * 1024;
  size_t max_gap_size = 10 * 1024 * 1024;
  size_t allow_before_gap_size = 1 * 1024 * 1024;

  std::string data(state.range(0), '0');

  for (auto _ : state) {
    state.PauseTiming();
    TDataStreamBufferImpl stream_buffer(capacity, max_gap_size, allow_before_gap_size);
    state.ResumeTiming();

    stream_buffer.Add(0, data, 1);
  }
  state.SetBytesProcessed(static_cast<uint64_t>(state.iterations()) * data.size());
}

template <typename TDataStreamBufferImpl>
// NOLINTNEXTLINE : runtime/references.
static void BM_OoOBytes(benchmark::State& state) {
  size_t capacity = 50 * 1024 * 1024;
  size_t max_gap_size = 10 * 1024 * 1024;
  size_t allow_before_gap_size = 1 * 1024 * 1024;

  std::string data(state.range(0), '0');

  int n_chunks = capacity / data.size();

  std::vector<size_t> chunk_pos;
  std::vector<uint64_t> chunk_ts;
  size_t pos = 0;
  size_t ts = 0;
  for (int i = 0; i < n_chunks; ++i) {
    chunk_pos.push_back(pos);
    chunk_ts.push_back(ts);
    pos += data.size();
    ts += 1;
  }

  std::vector<int> indices;
  for (int i = 0; i < n_chunks; ++i) {
    indices.push_back(i);
  }
  std::minstd_rand0 gen(0);
  std::shuffle(indices.begin(), indices.end(), gen);

  std::vector<size_t> shuffled_chunk_pos;
  std::vector<uint64_t> shuffled_chunk_ts;
  for (int i = 0; i < n_chunks; ++i) {
    shuffled_chunk_pos.push_back(chunk_pos[indices[i]]);
    shuffled_chunk_ts.push_back(chunk_ts[indices[i]]);
  }

  for (auto _ : state) {
    state.PauseTiming();
    TDataStreamBufferImpl stream_buffer(capacity, max_gap_size, allow_before_gap_size);
    state.ResumeTiming();

    // Add a full capacity worth of contiguous bytes.
    for (int i = 0; i < n_chunks; ++i) {
      stream_buffer.Add(shuffled_chunk_pos[i], data, shuffled_chunk_ts[i]);
    }

    // Access head.
    benchmark::DoNotOptimize(stream_buffer.Head());
  }
  state.SetBytesProcessed(static_cast<uint64_t>(state.iterations()) * n_chunks * data.size());
}

template <typename TDataStreamBufferImpl>
// NOLINTNEXTLINE : runtime/references.
static void BM_LargeGap(benchmark::State& state) {
  size_t capacity = 50 * 1024 * 1024;
  size_t max_gap_size = 10 * 1024 * 1024;
  size_t allow_before_gap_size = 1 * 1024 * 1024;

  std::string data(state.range(0), '0');

  int n_chunks = capacity / data.size();
  size_t gap_size = 8 * 1024 * 1024;

  for (auto _ : state) {
    state.PauseTiming();
    TDataStreamBufferImpl stream_buffer(capacity, max_gap_size, allow_before_gap_size);
    state.ResumeTiming();

    size_t pos = 0;
    uint64_t ts = 0;
    // Add a full capacity worth of contiguous bytes.
    for (int i = 0; i < n_chunks; ++i) {
      stream_buffer.Add(pos, data, ts);
      pos += data.size();
      ts += 1;
      if (i == (n_chunks / 2)) {
        pos += gap_size;
      }
    }

    // Access head.
    benchmark::DoNotOptimize(stream_buffer.Head());
  }
  state.SetBytesProcessed(static_cast<uint64_t>(state.iterations()) * n_chunks * data.size());
}

template <typename TDataStreamBufferImpl>
// NOLINTNEXTLINE : runtime/references.
static void BM_OverrunCapacity(benchmark::State& state) {
  size_t capacity = 50 * 1024 * 1024;
  size_t max_gap_size = 10 * 1024 * 1024;
  size_t allow_before_gap_size = 1 * 1024 * 1024;

  std::string data(state.range(0), '0');

  int n_chunks = capacity / data.size();
  // Double n_chunks to make the buffer go over capacity.
  n_chunks *= 2;

  for (auto _ : state) {
    state.PauseTiming();
    TDataStreamBufferImpl stream_buffer(capacity, max_gap_size, allow_before_gap_size);
    state.ResumeTiming();

    size_t pos = 0;
    uint64_t ts = 0;
    // Add a full capacity worth of contiguous bytes.
    for (int i = 0; i < n_chunks; ++i) {
      stream_buffer.Add(pos, data, ts);
      pos += data.size();
      ts += 1;
    }

    // Access head.
    benchmark::DoNotOptimize(stream_buffer.Head());
  }
  state.SetBytesProcessed(static_cast<uint64_t>(state.iterations()) * n_chunks * data.size());
}

template <typename TDataStreamBufferImpl>
// NOLINTNEXTLINE : runtime/references.
static void BM_RemovePrefix(benchmark::State& state) {
  size_t capacity = 50 * 1024 * 1024;
  size_t max_gap_size = 10 * 1024 * 1024;
  size_t allow_before_gap_size = 1 * 1024 * 1024;

  std::string data(128 * 1024, '0');

  int n_chunks = capacity / data.size();

  int n_remove_prefix = capacity / state.range(0);

  for (auto _ : state) {
    state.PauseTiming();
    TDataStreamBufferImpl stream_buffer(capacity, max_gap_size, allow_before_gap_size);

    size_t pos = 0;
    uint64_t ts = 0;
    // Add a full capacity worth of contiguous bytes.
    for (int i = 0; i < n_chunks; ++i) {
      stream_buffer.Add(pos, data, ts);
      pos += data.size();
      ts += 1;
    }
    benchmark::DoNotOptimize(stream_buffer.Head());
    state.ResumeTiming();

    for (int i = 0; i < n_remove_prefix; ++i) {
      stream_buffer.RemovePrefix(state.range(0));
    }
  }
}

using px::stirling::protocols::AlwaysContiguousDataStreamBufferImpl;
using px::stirling::protocols::LazyContiguousDataStreamBufferImpl;

BENCHMARK_TEMPLATE(BM_ContiguousBytes, LazyContiguousDataStreamBufferImpl)
    ->Range(1024, 32 * 1024)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_ContiguousBytes, AlwaysContiguousDataStreamBufferImpl)
    ->Range(1024, 32 * 1024)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_TEMPLATE(BM_SingleAdd, LazyContiguousDataStreamBufferImpl)->Range(1024, 32 * 1024);
BENCHMARK_TEMPLATE(BM_SingleAdd, AlwaysContiguousDataStreamBufferImpl)->Range(1024, 32 * 1024);

BENCHMARK_TEMPLATE(BM_OoOBytes, LazyContiguousDataStreamBufferImpl)
    ->Range(1024, 32 * 1024)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_OoOBytes, AlwaysContiguousDataStreamBufferImpl)
    ->Range(1024, 32 * 1024)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_TEMPLATE(BM_OverrunCapacity, LazyContiguousDataStreamBufferImpl)
    ->Range(1024, 32 * 1024)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_OverrunCapacity, AlwaysContiguousDataStreamBufferImpl)
    ->Range(32 * 1024, 32 * 1024)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_TEMPLATE(BM_LargeGap, LazyContiguousDataStreamBufferImpl)
    ->Range(1024, 32 * 1024)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_LargeGap, AlwaysContiguousDataStreamBufferImpl)
    ->Range(32 * 1024, 32 * 1024)
    ->Unit(benchmark::kMillisecond);

BENCHMARK_TEMPLATE(BM_RemovePrefix, LazyContiguousDataStreamBufferImpl)
    ->Range(1024, 32 * 1024)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_RemovePrefix, AlwaysContiguousDataStreamBufferImpl)
    ->Range(1024, 32 * 1024)
    ->Unit(benchmark::kMillisecond);
