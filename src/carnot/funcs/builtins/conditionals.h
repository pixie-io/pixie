#pragma once

#include "src/carnot/udf/registry.h"
#include "src/shared/types/types.h"

namespace pl {
namespace carnot {
namespace builtins {

/**
 * Registers UDF operations that work on conditionals
 * @param registry pointer to the registry.
 */
void RegisterConditionalOpsOrDie(udf::Registry* registry);

template <typename TArg>
class SelectUDF : public udf::ScalarUDF {
 public:
  TArg Exec(FunctionContext*, BoolValue s, TArg v1, TArg v2) {
    // TODO(zasgar): This does do eager evaluation of both sides./
    // If we start thinking about code generation, we can probably optimize this.
    if (s.val) {
      return v1;
    }
    return v2;
  }

  static udf::InfRuleVec SemanticInferenceRules() {
    // Match the 1st and 2nd arg.
    return {udf::InheritTypeFromArgs<SelectUDF>::CreateGeneric({1, 2})};
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Selects value based on the first argument.")
        .Example(R"doc(
        | # Explicit call.
        | df.val = px.select(df.select_left, df.left, df.right)
        )doc")
        .Arg("s", "The selector value")
        .Arg("v1", "The return value when s is true")
        .Arg("v2", "The return value when s is false")
        .Returns("Return v1 if s else v2");
  }
};

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
