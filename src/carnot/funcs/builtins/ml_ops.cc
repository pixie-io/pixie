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

#include "src/carnot/funcs/builtins/ml_ops.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace builtins {

void RegisterMLOpsOrDie(udf::Registry* registry) {
  CHECK(registry != nullptr);
  /*****************************************
   * Scalar UDFs.
   *****************************************/
  registry->RegisterOrDie<TransformerUDF>("_text_embedding");
  registry->RegisterOrDie<SentencePieceUDF>("_encode_sentence_piece");
  registry->RegisterOrDie<KMeansUDF>("_kmeans_inference");
  /*****************************************
   * Aggregate UDFs.
   *****************************************/
  registry->RegisterOrDie<KMeansUDA>("_kmeans_fit");
  registry->RegisterOrDie<ReservoirSampleUDA<types::StringValue>>("sample");
}

int load_floats_from_json(std::string in, Eigen::VectorXf* out, int max_num) {
  rapidjson::Document d;
  rapidjson::ParseResult ok = d.Parse(in.data());
  // TODO(zasgar/michellenguyen, PP-419): Replace with null when available.
  if (ok == nullptr) {
    return 0;
  }
  if (!d.IsArray()) {
    return 0;
  }

  int count = 0;
  for (rapidjson::Value::ConstValueIterator itr = d.Begin(); itr != d.End(); ++itr) {
    if (count == max_num) {
      return count;
    }
    const rapidjson::Value& val = *itr;
    if (!val.IsDouble()) {
      return 0;
    }
    out->operator()(count) = val.GetFloat();
    count++;
  }
  return count;
}

std::string write_ints_to_json(int* arr, int num) {
  // Copy output to json array.
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  writer.StartArray();
  for (int i = 0; i < num; i++) {
    writer.Int(arr[i]);
  }
  writer.EndArray();
  return sb.GetString();
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
