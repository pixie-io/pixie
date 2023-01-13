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

// This benchmark measures the performance of various ways of converting byte strings into
// primitive int types of varying lengths..

template <int...>
constexpr std::false_type always_false{};

// MemCopy is provided as a reference as an upper bound.
// It is not functionally correct on a big-endian machine.
template <typename T, size_t N = sizeof(T)>
T MemCopy(std::string_view buf) {
  // Doesn't make sense to process more bytes than the destination type.
  // Less bytes is okay, on the other hand, since the value will still fit.
  static_assert(N <= sizeof(T));

  // Source buffer must have enough bytes.
  DCHECK_GE(buf.size(), N);

  // This memcpy is required for memory alignment.
  T x = 0;
  memcpy(&x, buf.data(), N);

  return x;
}

// MemCopyLEtoHost is a version that uses memcpy() and lenntoh().
template <typename T, size_t N = sizeof(T)>
T MemCopyLEtoHost(std::string_view buf) {
  // Doesn't make sense to process more bytes than the destination type.
  // Less bytes is okay, on the other hand, since the value will still fit.
  static_assert(N <= sizeof(T));

  // Source buffer must have enough bytes.
  DCHECK_GE(buf.size(), N);

  // This memcpy is required for memory alignment.
  T x = 0;
  memcpy(&x, buf.data(), N);

  if constexpr (sizeof(T) == 8) {
    x = le64toh(x);
  } else if constexpr (sizeof(T) == 4) {
    x = le32toh(x);
  } else if constexpr (sizeof(T) == 2) {
    x = le16toh(x);
  } else if constexpr (sizeof(T) == 1) {
    // Nothing.
  } else {
    static_assert(always_false<sizeof(T)>, "Invalid template parameter.");
  }

  return x;
}

// ByteLoop performs the conversion through a loop.
// The number of bytes is communicated through the string_view parameter.
template <typename T>
T ByteLoop(std::string_view buf) {
  // Doesn't make sense to process more bytes than the destination type.
  // Less bytes is okay, on the other hand, since the value will still fit.
  DCHECK(buf.size() <= sizeof(T));

  T result = 0;
  for (size_t i = 0; i < buf.size(); i++) {
    result = static_cast<uint8_t>(buf[buf.size() - 1 - i]) + (result << 8);
  }
  return result;
}

// ByteLoopn performs the conversion through a loop.
// Unlike ByteLoop, the number of bytes is communicated explicitly.
template <typename T>
T ByteLoopn(std::string_view buf, uint32_t n = sizeof(T)) {
  // Doesn't make sense to process more bytes than the destination type.
  // Less bytes is okay, on the other hand, since the value will still fit.
  DCHECK(buf.size() <= sizeof(T));

  // Source buffer must have enough bytes.
  DCHECK_GE(buf.size(), n);

  T result = 0;
  for (size_t i = 0; i < n; i++) {
    result = static_cast<uint8_t>(buf[n - 1 - i]) + (result << 8);
  }
  return result;
}

// ByteLoopN performs the conversion through a loop.
// In this version, the number of bytes is communicated through a template parameter.
// This should enable the compiler to perform loop-unrolling and other optimizations.
template <typename T, size_t N = sizeof(T)>
T ByteLoopN(std::string_view buf) {
  // Doesn't make sense to process more bytes than the destination type.
  // Less bytes is okay, on the other hand, since the value will still fit.
  static_assert(N <= sizeof(T));

  // Source buffer must have enough bytes.
  DCHECK_GE(buf.size(), N);

  T result = 0;
  for (size_t i = 0; i < N; i++) {
    result = static_cast<uint8_t>(buf[N - 1 - i]) + (result << 8);
  }
  return result;
}

std::string InitBytes(int size) {
  std::string buf;
  buf.resize(size);
  for (int i = 0; i < size; ++i) {
    buf[i] = i;
  }
  return buf;
}

// Create a bunch of bytes.
// Currently, only up to the first 8 bytes are used.
// NOLINTNEXTLINE: runtime/string
const std::string kBuf = InitBytes(100);

template <typename T, size_t N = sizeof(T)>
// NOLINTNEXTLINE : runtime/references.
static void BM_MemCopy(benchmark::State& state) {
  for (auto _ : state) {
    auto result = MemCopy<T, N>(kBuf);
    benchmark::DoNotOptimize(result);
  }
}

template <typename T, size_t N = sizeof(T)>
// NOLINTNEXTLINE : runtime/references.
static void BM_MemCopyLEtoHost(benchmark::State& state) {
  for (auto _ : state) {
    auto result = MemCopyLEtoHost<T, N>(kBuf);
    benchmark::DoNotOptimize(result);
  }
}

template <typename T, size_t N = sizeof(T)>
// NOLINTNEXTLINE : runtime/references.
static void BM_ByteLoop(benchmark::State& state) {
  for (auto _ : state) {
    auto result = ByteLoop<T>(kBuf.substr(0, N));
    benchmark::DoNotOptimize(result);
  }
}

template <typename T, size_t N = sizeof(T)>
// NOLINTNEXTLINE : runtime/references.
static void BM_ByteLoopn(benchmark::State& state) {
  for (auto _ : state) {
    auto result = ByteLoopn<T>(kBuf, N);
    benchmark::DoNotOptimize(result);
  }
}

template <typename T, size_t N = sizeof(T)>
// NOLINTNEXTLINE : runtime/references.
static void BM_ByteLoopN(benchmark::State& state) {
  for (auto _ : state) {
    auto result = ByteLoopN<T, N>(kBuf);
    benchmark::DoNotOptimize(result);
  }
}

// BM_MemCopy is provided as a reference as an upper bound.
// It is not functionally correct on big-endian machines.
BENCHMARK_TEMPLATE(BM_MemCopy, uint64_t);
BENCHMARK_TEMPLATE(BM_MemCopy, uint64_t, 6);
BENCHMARK_TEMPLATE(BM_MemCopy, uint32_t);
BENCHMARK_TEMPLATE(BM_MemCopy, uint32_t, 3);
BENCHMARK_TEMPLATE(BM_MemCopy, uint16_t);
BENCHMARK_TEMPLATE(BM_MemCopy, uint8_t);

BENCHMARK_TEMPLATE(BM_MemCopyLEtoHost, uint64_t);
BENCHMARK_TEMPLATE(BM_MemCopyLEtoHost, uint64_t, 6);
BENCHMARK_TEMPLATE(BM_MemCopyLEtoHost, uint32_t);
BENCHMARK_TEMPLATE(BM_MemCopyLEtoHost, uint32_t, 3);
BENCHMARK_TEMPLATE(BM_MemCopyLEtoHost, uint16_t);
BENCHMARK_TEMPLATE(BM_MemCopyLEtoHost, uint8_t);

BENCHMARK_TEMPLATE(BM_ByteLoopN, uint64_t);
BENCHMARK_TEMPLATE(BM_ByteLoopN, uint64_t, 6);
BENCHMARK_TEMPLATE(BM_ByteLoopN, uint32_t);
BENCHMARK_TEMPLATE(BM_ByteLoopN, uint32_t, 3);
BENCHMARK_TEMPLATE(BM_ByteLoopN, uint16_t);
BENCHMARK_TEMPLATE(BM_ByteLoopN, uint8_t);

BENCHMARK_TEMPLATE(BM_ByteLoopn, uint64_t);
BENCHMARK_TEMPLATE(BM_ByteLoopn, uint64_t, 6);
BENCHMARK_TEMPLATE(BM_ByteLoopn, uint32_t);
BENCHMARK_TEMPLATE(BM_ByteLoopn, uint32_t, 3);
BENCHMARK_TEMPLATE(BM_ByteLoopn, uint16_t);
BENCHMARK_TEMPLATE(BM_ByteLoopn, uint8_t);

BENCHMARK_TEMPLATE(BM_ByteLoop, uint64_t);
BENCHMARK_TEMPLATE(BM_ByteLoop, uint64_t, 6);
BENCHMARK_TEMPLATE(BM_ByteLoop, uint32_t);
BENCHMARK_TEMPLATE(BM_ByteLoop, uint32_t, 3);
BENCHMARK_TEMPLATE(BM_ByteLoop, uint16_t);
BENCHMARK_TEMPLATE(BM_ByteLoop, uint8_t);
