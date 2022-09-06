/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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
  registry->RegisterOrDie<HeapRangesUDTF>("_HeapRanges");
  registry->RegisterOrDie<HeapStatsNumericUDTF>("_HeapStatsNumeric");

  registry->RegisterOrDie<AgentProcStatusUDTF>("_DebugAgentProcStatus");
  registry->RegisterOrDie<AgentProcSMapsUDTF>("_DebugAgentProcSMaps");

  registry->RegisterOrDie<HeapReleaseFreeMemoryUDTF>("_HeapReleaseFreeMemory");
}

}  // namespace internal
}  // namespace funcs
}  // namespace vizier
}  // namespace px
