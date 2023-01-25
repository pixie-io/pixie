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

#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/vizier/funcs/context/vizier_context.h"
#include "src/vizier/funcs/funcs.h"

namespace px {
namespace carnot {
namespace udfexporter {

StatusOr<std::unique_ptr<planner::RegistryInfo>> ExportUDFInfo() {
  auto registry = std::make_unique<udf::Registry>("udf_registry");

  vizier::funcs::VizierFuncFactoryContext ctx;
  vizier::funcs::RegisterFuncsOrDie(ctx, registry.get());

  udfspb::UDFInfo udf_proto = registry->ToProto();
  auto registry_info = std::make_unique<planner::RegistryInfo>();
  PX_RETURN_IF_ERROR(registry_info->Init(udf_proto));
  return registry_info;
}

udfspb::Docs ExportUDFDocs() {
  udf::Registry registry("udf_registry");

  vizier::funcs::VizierFuncFactoryContext ctx;
  vizier::funcs::RegisterFuncsOrDie(ctx, &registry);
  return registry.ToDocsProto();
}

}  // namespace udfexporter
}  // namespace carnot
}  // namespace px
