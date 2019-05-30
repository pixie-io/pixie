#pragma once

#include "src/carnot/udf/registry.h"
#include "src/shared/types/types.h"

namespace pl {
namespace carnot {
namespace builtins {

class ContainsUDF : public udf::ScalarUDF {
 public:
  types::BoolValue Exec(udf::FunctionContext*, types::StringValue b1, types::StringValue b2) {
    return absl::StrContains(b1, b2);
  }
};

class LengthUDF : public udf::ScalarUDF {
 public:
  types::Int64Value Exec(udf::FunctionContext*, types::StringValue b1) { return b1.length(); }
};

void RegisterStringOpsOrDie(udf::ScalarUDFRegistry* registry);
}  // namespace builtins
}  // namespace carnot
}  // namespace pl
