#include "src/vizier/funcs/internal/internal_impl.h"
#include "src/vizier/funcs/internal/debug.h"
#include "src/vizier/funcs/internal/stack_trace.h"

namespace px {
namespace vizier {
namespace funcs {
namespace internal {

void RegisterFuncsOrDie(carnot::udf::Registry* registry) {
  registry->RegisterOrDie<StackTracerUDTF>("_DebugStackTrace");
  registry->RegisterOrDie<KelvinVersionUDTF>("Version");
  registry->RegisterOrDie<HeapStatsUDTF>("_HeapStats");
  registry->RegisterOrDie<HeapSampleUDTF>("_HeapSample");
  registry->RegisterOrDie<HeapGrowthStacksUDTF>("_HeapGrowthStacks");
}

}  // namespace internal
}  // namespace funcs
}  // namespace vizier
}  // namespace px
