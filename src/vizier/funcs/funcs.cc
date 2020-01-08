#include "src/vizier/funcs/funcs.h"
#include "src/carnot/funcs/funcs.h"

namespace pl {
namespace vizier {
namespace funcs {

void RegisterFuncsOrDie(carnot::udf::Registry* registry) {
  // All used functions must be registered here.
  ::pl::carnot::funcs::RegisterFuncsOrDie(registry);
}

}  // namespace funcs
}  // namespace vizier
}  // namespace pl
