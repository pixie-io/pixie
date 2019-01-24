#include <glog/logging.h>

#include "src/carnot/builtins/math_ops.h"
#include "src/carnot/udf/registry.h"

namespace pl {
namespace carnot {
namespace builtins {

void RegisterMathOpsOrDie(udf::ScalarUDFRegistry* registry) {
  CHECK(registry != nullptr);
  registry->RegisterOrDie<AddUDF<udf::Int64Value, udf::Int64Value, udf::Int64Value>>("add");
  registry->RegisterOrDie<AddUDF<udf::Float64Value, udf::Float64Value, udf::Int64Value>>("add");
  registry->RegisterOrDie<AddUDF<udf::Float64Value, udf::Int64Value, udf::Float64Value>>("add");
  registry->RegisterOrDie<AddUDF<udf::Float64Value, udf::Float64Value, udf::Float64Value>>("add");
}

void RegisterMathOpsOrDie(udf::UDARegistry* registry) {
  CHECK(registry != nullptr);
  registry->RegisterOrDie<MeanUDA<udf::Float64Value>>("mean");
  registry->RegisterOrDie<MeanUDA<udf::Int64Value>>("mean");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
