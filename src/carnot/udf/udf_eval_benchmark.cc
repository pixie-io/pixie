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
#include <vector>

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf_wrapper.h"
#include "src/common/base/base.h"
#include "src/common/benchmark/benchmark.h"
#include "src/datagen/datagen.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"

using px::Status;
using px::carnot::udf::FunctionContext;
using px::carnot::udf::ScalarUDF;
using px::carnot::udf::ScalarUDFDefinition;
using px::carnot::udf::ScalarUDFWrapper;
using px::types::BaseValueType;
using px::types::Int64Value;
using px::types::Int64ValueColumnWrapper;
using px::types::StringValue;
using px::types::StringValueColumnWrapper;
using px::types::ToArrow;

using px::datagen::CreateLargeData;
using px::datagen::RandomString;

std::vector<StringValue> GenerateStringValueVector(int size, int string_width) {
  std::vector<StringValue> data(size);

  std::generate(begin(data), end(data), [=] { return RandomString(string_width); });
  return data;
}

class AddUDF : public ScalarUDF {
 public:
  Int64Value Exec(FunctionContext*, Int64Value v1, Int64Value v2) { return v1.val + v2.val; }
};

class SubStrUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue v1) { return v1.substr(1, 2); }
};

// This benchmark add two columns using Int64ValueVectors.
// NOLINTNEXTLINE : runtime/references.
static void BM_AddInt64Values(benchmark::State& state) {
  auto vec1 = CreateLargeData<Int64Value>(state.range(0));
  auto vec2 = CreateLargeData<Int64Value>(state.range(0));
  Int64ValueColumnWrapper out(vec2.size());

  // Convert to type erased wrapper.
  auto wrapped_vec1 = Int64ValueColumnWrapper(vec1);
  auto wrapped_vec2 = Int64ValueColumnWrapper(vec2);

  // Create the UDF.
  ScalarUDFDefinition def("add");
  CHECK(def.template Init<AddUDF>().ok());
  auto u = def.Make();

  // Loop the test.
  // NOLINTNEXTLINE : clang-analyzer-deadcode.DeadStores.
  for (auto _ : state) {
    out.Resize(vec2.size());
    auto res = def.ExecBatch(u.get(), nullptr, {&wrapped_vec1, &wrapped_vec2}, &out, vec1.size());
    CHECK(res.ok());
    benchmark::DoNotOptimize(out);
    out.Clear();
  }

  // Check results.
  for (size_t idx = 0; idx < vec2.size(); ++idx) {
    CHECK((vec1[idx].val + vec2[idx].val) == out[idx].val);
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * 2 * vec1.size() * sizeof(int64_t));
}

// This benchmark performs a substring on 10 char wide strings,
// selects two characters.
// NOLINTNEXTLINE : runtime/references.
static void BM_SubStr(benchmark::State& state) {
  int width = 10;
  auto vec1 = GenerateStringValueVector(state.range(0), width);
  StringValueColumnWrapper out(vec1.size());
  auto wrapped_vec1 = StringValueColumnWrapper(vec1);

  // Create UDF.
  ScalarUDFDefinition def("substr");
  CHECK(def.template Init<SubStrUDF>().ok());
  auto u = def.Make();

  // Run the test.
  // NOLINTNEXTLINE : clang-analyzer-deadcode.DeadStores.
  for (auto _ : state) {
    auto res = def.ExecBatch(u.get(), nullptr, {&wrapped_vec1}, &out, vec1.size());
    PX_CHECK_OK(res);
    benchmark::DoNotOptimize(out);
  }

  // Check results.
  for (size_t idx = 0; idx < vec1.size(); ++idx) {
    CHECK(vec1[idx].substr(1, 2) == out[idx]);
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * width * vec1.size());
}

// Benchmark adding two integers using arrow as the interface.
// NOLINTNEXTLINE : runtime/references.
static void BM_AddTwoInt64sArrow(benchmark::State& state) {
  size_t size = state.range(0);
  auto arr1 = ToArrow(CreateLargeData<Int64Value>(size), arrow::default_memory_pool());
  auto arr2 = ToArrow(CreateLargeData<Int64Value>(size), arrow::default_memory_pool());

  auto u = std::make_shared<AddUDF>();
  std::shared_ptr<arrow::Array> out;
  // NOLINTNEXTLINE : clang-analyzer-deadcode.DeadStores.
  for (auto _ : state) {
    if (out) {
      out.reset();
    }
    auto output_builder = std::make_shared<arrow::Int64Builder>();
    auto res = ScalarUDFWrapper<AddUDF>::ExecBatchArrow(u.get(), nullptr, {arr1.get(), arr2.get()},
                                                        output_builder.get(), size);
    CHECK(res.ok());
    CHECK(output_builder->Finish(&out).ok());
    benchmark::DoNotOptimize(out);
  }

  // Check results.
  auto arr1_casted = static_cast<arrow::Int64Array*>(arr1.get());
  auto arr2_casted = static_cast<arrow::Int64Array*>(arr2.get());
  auto out_casted = static_cast<arrow::Int64Array*>(out.get());
  for (size_t idx = 0; idx < size; ++idx) {
    CHECK(arr1_casted->Value(idx) + arr2_casted->Value(idx) == out_casted->Value(idx));
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * sizeof(int64_t) * 2 * size);
}

// Benchmark converting Int64 to Arrow.
// NOLINTNEXTLINE : runtime/references.
static void BM_ConvertToArrowInt64(benchmark::State& state) {
  size_t size = state.range(0);
  auto data = CreateLargeData<Int64Value>(size);
  // NOLINTNEXTLINE : clang-analyzer-deadcode.DeadStores.
  for (auto _ : state) {
    auto arrow_array = ToArrow(data, arrow::default_memory_pool());
    benchmark::DoNotOptimize(arrow_array);
  }
  state.SetBytesProcessed(int64_t(state.iterations()) * sizeof(int64_t) * size);
}

// Benchmark converting strings to Arrow.
// NOLINTNEXTLINE : runtime/references.
static void BM_ConvertToArrowString(benchmark::State& state) {
  int width = 10;
  auto data = GenerateStringValueVector(state.range(0), width);
  // NOLINTNEXTLINE : clang-analyzer-deadcode.DeadStores.
  for (auto _ : state) {
    auto arrow_array = ToArrow(data, arrow::default_memory_pool());
    benchmark::DoNotOptimize(arrow_array);
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * width * data.size());
}

// Benchmark adding two Int64 values and producing an arrow result.
// NOLINTNEXTLINE : runtime/references.
static void BM_AddInt64ValueToArrow(benchmark::State& state) {
  auto vec1 = CreateLargeData<Int64Value>(state.range(0));
  auto vec2 = CreateLargeData<Int64Value>(state.range(0));
  Int64ValueColumnWrapper out(vec2.size());

  auto wrapped_vec1 = Int64ValueColumnWrapper(vec1);
  auto wrapped_vec2 = Int64ValueColumnWrapper(vec2);

  ScalarUDFDefinition def("add");
  CHECK(def.template Init<AddUDF>().ok());
  auto u = def.Make();
  // NOLINTNEXTLINE : clang-analyzer-deadcode.DeadStores.
  for (auto _ : state) {
    out.Clear();
    out.Resize(vec2.size());
    auto res = def.ExecBatch(u.get(), nullptr, {&wrapped_vec1, &wrapped_vec2}, &out, vec1.size());
    CHECK(res.ok());
    auto arrow_res = out.ConvertToArrow(arrow::default_memory_pool());
    benchmark::DoNotOptimize(arrow_res);
  }

  // Check results.
  for (size_t idx = 0; idx < vec2.size(); ++idx) {
    CHECK((vec1[idx].val + vec2[idx].val) == out[idx].val);
  }

  state.SetBytesProcessed(int64_t(state.iterations()) * 2 * vec1.size() * sizeof(int64_t));
}

// Benchmark doing substring on arrow.
// NOLINTNEXTLINE : runtime/references.
static void BM_SubStrArrow(benchmark::State& state) {
  int width = 10;
  auto data = GenerateStringValueVector(state.range(0), width);
  auto in_arr = ToArrow(data, arrow::default_memory_pool());

  // Create UDF.
  std::shared_ptr<arrow::Array> out;
  ScalarUDFDefinition def("substr");
  CHECK(def.template Init<SubStrUDF>().ok());
  auto u = def.Make();
  // NOLINTNEXTLINE : clang-analyzer-deadcode.DeadStores.
  for (auto _ : state) {
    if (out) {
      out.reset();
    }
    auto output_builder = std::make_shared<arrow::StringBuilder>();
    auto res = ScalarUDFWrapper<SubStrUDF>::ExecBatchArrow(u.get(), nullptr, {in_arr.get()},
                                                           output_builder.get(), data.size());
    CHECK(res.ok());
    CHECK(output_builder->Finish(&out).ok());
    benchmark::DoNotOptimize(out);
  }
  state.SetBytesProcessed(int64_t(state.iterations()) * width * data.size());
}

BENCHMARK(BM_AddInt64ValueToArrow)->RangeMultiplier(2)->Range(1, 1 << 16);
BENCHMARK(BM_AddTwoInt64sArrow)->RangeMultiplier(2)->Range(1, 1 << 16);
BENCHMARK(BM_AddInt64Values)->RangeMultiplier(2)->Range(1, 1 << 16);

BENCHMARK(BM_ConvertToArrowString)->RangeMultiplier(2)->Range(1, 1 << 16);
BENCHMARK(BM_ConvertToArrowInt64)->RangeMultiplier(2)->Range(1, 1 << 16);

BENCHMARK(BM_SubStrArrow)->RangeMultiplier(2)->Range(1, 1 << 16);
BENCHMARK(BM_SubStr)->RangeMultiplier(2)->Range(1, 1 << 16);
