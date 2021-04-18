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

#include "src/carnot/exec/ml/transformer_executor.h"

namespace px {
namespace carnot {
namespace exec {
namespace ml {

static int load_ints_from_json(std::string in, int32_t* arr, int max_num) {
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
    if (!val.IsInt()) {
      return 0;
    }
    arr[count] = val.GetInt();
    count++;
  }
  return count;
}

void TransformerExecutor::Execute(std::string doc, std::string* out) {
  // LOG(INFO) << "inputs: " << tf_interpreter_->inputs().size() << "\n";
  // LOG(INFO) << "input(0) name: " << tf_interpreter_->GetInputName(0) << "\n";

  // int t_size = tf_interpreter_->tensors_size();
  // for (int i = 0; i < t_size; i++) {
  //   if (tf_interpreter_->tensor(i)->name) {
  //     LOG(INFO) << i << ": " << tf_interpreter_->tensor(i)->name << ", "
  //               << tf_interpreter_->tensor(i)->bytes << ", "
  //               << tf_interpreter_->tensor(i)->type;
  //   }
  // }
  auto input = tf_interpreter_->typed_input_tensor<int32_t>(0);
  if (input == nullptr) {
    LOG(INFO) << "Error getting typed input tensor, most likely using wrong type for this model";
    *out = "";
    return;
  }

  auto count = load_ints_from_json(doc, input, max_length_);
  if (count == 0) {
    // Either input array was empty or there was an error parsing the json, either way don't
    // continue.
    *out = "";
    return;
  }

  // Add 1 to each token to account for pad token.
  for (int i = 0; i < count; i++) {
    input[i] = input[i] + 1;
  }

  for (int i = count; i < max_length_; i++) {
    input[i] = 0;
  }

  const int embedding_size = 256;

  tf_interpreter_->Invoke();

  auto output = tf_interpreter_->typed_output_tensor<float>(0);

  // Copy output to json array.
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  writer.StartArray();
  for (int i = 0; i < embedding_size; i++) {
    writer.Double(output[i]);
  }
  writer.EndArray();
  *out = sb.GetString();
}

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace px
