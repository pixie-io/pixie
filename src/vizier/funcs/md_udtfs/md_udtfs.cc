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

#include "src/vizier/funcs/md_udtfs/md_udtfs.h"

#include "src/carnot/udf/udf.h"
#include "src/common/base/base.h"
#include "src/vizier/funcs/context/vizier_context.h"
#include "src/vizier/funcs/md_udtfs/md_udtfs_impl.h"

namespace px {
namespace vizier {
namespace funcs {
namespace md {

void RegisterFuncsOrDie(const VizierFuncFactoryContext& ctx, carnot::udf::Registry* registry) {
  registry->RegisterFactoryOrDie<GetTables, UDTFWithMDFactory<GetTables>>("GetTables", ctx);
  registry->RegisterFactoryOrDie<GetTableSchemas, UDTFWithMDFactory<GetTableSchemas>>("GetSchemas",
                                                                                      ctx);
  registry->RegisterFactoryOrDie<GetAgentStatus, UDTFWithMDFactory<GetAgentStatus>>(
      "GetAgentStatus", ctx);
  registry->RegisterFactoryOrDie<GetProfilerSamplingPeriodMS,
                                 UDTFWithMDFactory<GetProfilerSamplingPeriodMS>>(
      "GetProfilerSamplingPeriodMS", ctx);

  registry->RegisterOrDie<GetDebugMDState>("_DebugMDState");
  registry->RegisterFactoryOrDie<GetDebugMDWithPrefix, UDTFWithMDFactory<GetDebugMDWithPrefix>>(
      "_DebugMDGetWithPrefix", ctx);
  registry->RegisterFactoryOrDie<GetDebugTableInfo, UDTFWithTableStoreFactory<GetDebugTableInfo>>(
      "_DebugTableInfo", ctx.table_store());

  registry->RegisterFactoryOrDie<GetUDFList, UDTFWithRegistryFactory<GetUDFList>>("GetUDFList",
                                                                                  registry);
  registry->RegisterFactoryOrDie<GetUDAList, UDTFWithRegistryFactory<GetUDAList>>("GetUDAList",
                                                                                  registry);
  registry->RegisterFactoryOrDie<GetUDTFList, UDTFWithRegistryFactory<GetUDTFList>>("GetUDTFList",
                                                                                    registry);

  registry->RegisterFactoryOrDie<GetTracepointStatus, UDTFWithMDTPFactory<GetTracepointStatus>>(
      "GetTracepointStatus", ctx);
  registry
      ->RegisterFactoryOrDie<GetCronScriptHistory, UDTFWithCronscriptFactory<GetCronScriptHistory>>(
          "GetCronScriptHistory", ctx);
}

}  // namespace md
}  // namespace funcs
}  // namespace vizier
}  // namespace px
