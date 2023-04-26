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

#include <vector>

#include "src/carnot/funcs/builtins/math_sketches.h"

namespace px {
namespace carnot {
namespace builtins {

void RegisterMathSketchesOrDie(udf::Registry* registry) {
  registry->RegisterOrDie<QuantilesUDA<types::Int64Value>>("quantiles");
  registry->RegisterOrDie<QuantilesUDA<types::Float64Value>>("quantiles");
}

void WriteCentroidArray(rapidjson::Writer<rapidjson::StringBuffer>* writer,
                        const std::vector<tdigest::Centroid>& centroids) {
  writer->StartArray();
  for (const auto& c : centroids) {
    writer->StartArray();
    writer->Double(c.mean());
    writer->Double(c.weight());
    writer->EndArray();
  }
  writer->EndArray();
}

std::vector<tdigest::Centroid> CentroidArrayFromJSON(const rapidjson::Value& val) {
  std::vector<tdigest::Centroid> centroids;
  for (rapidjson::Value::ConstValueIterator centroid = val.Begin(); centroid != val.End();
       ++centroid) {
    auto mean = centroid->GetArray()[0].GetDouble();
    auto weight = centroid->GetArray()[1].GetDouble();
    centroids.emplace_back(mean, weight);
  }
  return centroids;
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
