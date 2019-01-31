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
  // Mean
  registry->RegisterOrDie<MeanUDA<udf::Float64Value>>("mean");
  registry->RegisterOrDie<MeanUDA<udf::Int64Value>>("mean");
  // Sum
  registry->RegisterOrDie<SumUDA<udf::Float64Value>>("sum");
  registry->RegisterOrDie<SumUDA<udf::Int64Value>>("sum");
  // Max
  registry->RegisterOrDie<MaxUDA<udf::Float64Value>>("max");
  registry->RegisterOrDie<MaxUDA<udf::Int64Value>>("max");
  // Min
  registry->RegisterOrDie<MinUDA<udf::Float64Value>>("min");
  registry->RegisterOrDie<MinUDA<udf::Int64Value>>("min");
  // Count
  registry->RegisterOrDie<CountUDA<udf::Float64Value>>("count");
  registry->RegisterOrDie<CountUDA<udf::Int64Value>>("count");
  registry->RegisterOrDie<CountUDA<udf::BoolValue>>("count");
  registry->RegisterOrDie<CountUDA<udf::StringValue>>("count");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
