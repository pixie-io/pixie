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

#include "src/carnot/exec/ml/coreset.h"
#include "src/carnot/exec/ml/kmeans.h"
#include "src/common/perf/perf.h"

using px::carnot::exec::ml::KMeans;
using px::carnot::exec::ml::WeightedPointSet;

// NOLINTNEXTLINE : runtime/references.
static void BM_KMeansFit(benchmark::State& state) {
  int k = 10;
  int d = 64;
  KMeans kmeans(k);

  Eigen::MatrixXf points = Eigen::MatrixXf::Random(500, d);
  Eigen::VectorXf weights = Eigen::VectorXf::Random(500);
  auto set = std::make_shared<WeightedPointSet>(points, weights);

  for (auto _ : state) {
    kmeans.Fit(set);
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_KMeansTransform(benchmark::State& state) {
  int k = 10;
  int d = 64;
  KMeans kmeans(k);

  Eigen::MatrixXf points = Eigen::MatrixXf::Random(1000, d);
  Eigen::VectorXf weights = Eigen::VectorXf::Random(1000);
  auto set = std::make_shared<WeightedPointSet>(points, weights);
  kmeans.Fit(set);

  Eigen::VectorXf point = Eigen::VectorXf::Random(d);

  for (auto _ : state) {
    benchmark::DoNotOptimize(kmeans.Transform(point));
  }
}

BENCHMARK(BM_KMeansFit);
BENCHMARK(BM_KMeansTransform);
