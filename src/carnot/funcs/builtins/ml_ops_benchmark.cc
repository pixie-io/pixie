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

#include <gflags/gflags.h>

#include <benchmark/benchmark.h>
#include "src/carnot/exec/ml/transformer_executor.h"
#include "src/carnot/funcs/builtins/ml_ops.h"
#include "src/carnot/udf/model_pool.h"
#include "src/common/perf/perf.h"

DEFINE_string(sentencepiece_dir, "", "Path to sentencepiece.proto");
DEFINE_string(embedding_dir, "", "Path to embedding.proto");

std::vector<int> random_ints(std::size_t length) {
  std::random_device random_device;
  std::mt19937 generator(random_device());
  std::uniform_int_distribution<> distribution(0, 10000);

  std::vector<int> ints;

  for (std::size_t i = 0; i < length; ++i) {
    ints.push_back(distribution(generator));
  }

  return ints;
}

std::string random_string(std::size_t length) {
  const std::string CHARACTERS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

  std::random_device random_device;
  std::mt19937 generator(random_device());
  std::uniform_int_distribution<> distribution(0, CHARACTERS.size() - 1);

  std::string random_string;

  for (std::size_t i = 0; i < length; ++i) {
    random_string += CHARACTERS[distribution(generator)];
  }

  return random_string;
}

// NOLINTNEXTLINE : runtime/references.
static void BM_TransformerModel(benchmark::State& state) {
  px::carnot::builtins::TransformerUDF udf(FLAGS_embedding_dir);
  auto ints = random_ints(64);
  auto json = px::carnot::builtins::write_ints_to_json(ints.data(), 64);
  auto model_pool = px::carnot::udf::ModelPool::Create();
  auto model =
      model_pool->GetModelExecutor<px::carnot::exec::ml::TransformerExecutor>(FLAGS_embedding_dir);
  model.reset();
  auto ctx = px::carnot::udf::FunctionContext(nullptr, model_pool.get());

  for (auto _ : state) {
    benchmark::DoNotOptimize(udf.Exec(&ctx, json));
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_SentencePiece(benchmark::State& state) {
  auto udf = px::carnot::builtins::SentencePieceUDF(FLAGS_sentencepiece_dir);
  auto text = random_string(1024);

  for (auto _ : state) {
    benchmark::DoNotOptimize(udf.Exec(nullptr, text));
  }
}

BENCHMARK(BM_SentencePiece)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_TransformerModel)->Unit(benchmark::kMillisecond);
