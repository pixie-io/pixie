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

#pragma once

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/model.h>
#include <memory>
#include <string>
#include "src/carnot/udf/model_executor.h"
#include "src/common/base/utils.h"

namespace px {
namespace carnot {
namespace exec {
namespace ml {

class TransformerExecutor : public udf::ModelExecutor {
 public:
  TransformerExecutor() : TransformerExecutor("/embedding.proto") {}
  explicit TransformerExecutor(std::string model_proto_path) { Init(model_proto_path); }

  static constexpr udf::ModelType Type() { return udf::kTransformer; }

  void Init(std::string model_proto_path) {
    model_ = tflite::FlatBufferModel::BuildFromFile(model_proto_path.c_str());
    tflite::ops::builtin::BuiltinOpResolver resolver;
    tflite::InterpreterBuilder(*model_, resolver)(&tf_interpreter_);
    tf_interpreter_->ResizeInputTensor(tf_interpreter_->inputs()[0], {1, max_length_});
    if (tf_interpreter_->AllocateTensors() != kTfLiteOk) {
      LOG(INFO) << "Failed to allocate tensors";
    } else {
      LOG(INFO) << "Init Transformer model";
    }
  }

  void Execute(std::string doc, std::string* out);

 private:
  std::unique_ptr<tflite::Interpreter> tf_interpreter_;
  std::unique_ptr<tflite::FlatBufferModel> model_;
  int max_length_ = 64;
};

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace px
