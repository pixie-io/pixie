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

class FindUDF : public udf::ScalarUDF {
 public:
  types::Int64Value Exec(udf::FunctionContext*, types::StringValue src, types::StringValue substr) {
    return src.find(substr);
  }
};

class SubstringUDF : public udf::ScalarUDF {
 public:
  types::StringValue Exec(udf::FunctionContext*, types::StringValue b1, types::Int64Value pos,
                          types::Int64Value length) {
    return b1.substr(static_cast<size_t>(pos.val), static_cast<size_t>(length.val));
  }
};

void RegisterStringOpsOrDie(udf::ScalarUDFRegistry* registry);
}  // namespace builtins
}  // namespace carnot
}  // namespace pl
