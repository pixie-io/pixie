#include "src/carnot/funcs/builtins/math_sketches.h"

namespace px {
namespace carnot {
namespace builtins {

void RegisterMathSketchesOrDie(udf::Registry* registry) {
  registry->RegisterOrDie<QuantilesUDA<types::Int64Value>>("quantiles");
  registry->RegisterOrDie<QuantilesUDA<types::Float64Value>>("quantiles");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
