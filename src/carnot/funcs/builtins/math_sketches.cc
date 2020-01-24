#include "src/carnot/funcs/builtins/math_sketches.h"

namespace pl {
namespace carnot {
namespace builtins {

void RegisterMathSketchesOrDie(udf::Registry* registry) {
  registry->RegisterOrDie<QuantilesUDA<types::Int64Value>>("px.quantiles");
  registry->RegisterOrDie<QuantilesUDA<types::Float64Value>>("px.quantiles");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
