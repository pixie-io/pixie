#pragma once
#include "src/carnot/udf/registry.h"
#include "src/shared/types/types.h"

namespace pl {
namespace carnot {
namespace builtins {
/**
 * Registers UDF operations that work on collections
 * @param registry pointer to the registry.
 */
void RegisterCollectionOpsOrDie(udf::Registry* registry);

template <typename TArg>
class AnyUDA : public udf::UDA {
 public:
  AnyUDA() = default;
  void Update(FunctionContext*, TArg val) {
    // TODO(zasgar): We should find a way to short-circuit the agg since we only care
    // about one value.
    if (!picked) {
      val_ = val;
      picked = true;
    }
  }

  void Merge(FunctionContext*, const AnyUDA&) {
    // Does nothing.
  }

  TArg Finalize(FunctionContext*) { return val_; }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::InheritTypeFromArgs<AnyUDA>::CreateGeneric()};
  }

  static udf::UDADocBuilder Doc() {
    return udf::UDADocBuilder("Picks any single value.")
        .Details("Picks a value from the collection. No guarantees on which value is picked.")
        .Example(R"doc(
        | # Calculate any value from the collection.
        | df = df.agg(latency_dist=('val', px.any))
        )doc")
        .Arg("val", "The data to select the value from.")
        .Returns("The a single record selected from the above val.");
  }

 protected:
  TArg val_;
  bool picked = false;
};

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
