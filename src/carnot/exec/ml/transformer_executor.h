#pragma once

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/model.h>
#include <memory>
#include <string>
#include "src/carnot/exec/ml/model_executor.h"
#include "src/common/base/utils.h"

namespace pl {
namespace carnot {
namespace exec {
namespace ml {

class TransformerExecutor : public ModelExecutor {
 public:
  TransformerExecutor() : TransformerExecutor("/embedding.proto") {}
  explicit TransformerExecutor(std::string model_proto_path) { Init(model_proto_path); }

  static constexpr ModelType Type() { return kTransformer; }

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
}  // namespace pl
