#include "src/vizier/funcs/internal/internal_impl.h"
#include "src/vizier/funcs/internal/stack_trace.h"

namespace pl {
namespace vizier {
namespace funcs {
namespace internal {

void RegisterFuncsOrDie(carnot::udf::Registry* registry) {
  registry->RegisterOrDie<StackTracerUDTF>("_DebugStackTrace");
  registry->RegisterOrDie<KelvinVersionUDTF>("Version");
}

}  // namespace internal
}  // namespace funcs
}  // namespace vizier
}  // namespace pl
