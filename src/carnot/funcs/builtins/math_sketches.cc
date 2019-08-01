#include "src/carnot/funcs/builtins/math_sketches.h"

namespace pl {
namespace carnot {
namespace builtins {

void RegisterMathSketchesOrDie(udf::UDARegistry* registry) {
  registry->RegisterOrDie<QuantilesUDA<types::Int64Value>>("pl.quantiles");
  registry->RegisterOrDie<QuantilesUDA<types::Float64Value>>("pl.quantiles");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
