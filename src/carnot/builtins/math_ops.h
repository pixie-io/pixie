#pragma once

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"

namespace pl {
namespace carnot {
namespace builtins {

template <typename TReturn, typename TArg1, typename TArg2>
class AddUDF : udf::ScalarUDF {
 public:
  TReturn Exec(udf::FunctionContext *, TArg1 b1, TArg2 b2) { return b1.val + b2.val; }
};

template <typename TArg>
class MeanUDA : udf::UDA {
 public:
  void Update(udf::FunctionContext *, TArg arg) {
    size_++;
    count_ += arg.val;
  }
  void Merge(udf::FunctionContext *, const MeanUDA &other) {
    size_ += other.size_;
    count_ += other.count_;
  }
  udf::Float64Value Finalize(udf::FunctionContext *) { return count_ / size_; }

 protected:
  uint64_t size_ = 0;
  double count_ = 0;
};

void RegisterMathOpsOrDie(udf::ScalarUDFRegistry *registry);
void RegisterMathOpsOrDie(udf::UDARegistry *registry);
}  // namespace builtins
}  // namespace carnot
}  // namespace pl
