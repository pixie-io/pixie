#pragma once

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"

namespace pl {
namespace carnot {
namespace builtins {

template <typename TReturn, typename TArg1, typename TArg2>
class AddUDF : udf::ScalarUDF {
 public:
  TReturn Exec(udf::FunctionContext*, TArg1 b1, TArg2 b2) { return b1.val + b2.val; }
};

void RegisterMathOpsOrDie(udf::ScalarUDFRegistry* registry);

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
