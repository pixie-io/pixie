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

#pragma once
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/planner/compiler_state/registry_info.h"
#include "src/carnot/udf/registry.h"

namespace px {
namespace carnot {
namespace udfexporter {

/**
 * @brief ExportUDFInfo() setups a RegistryInfo using all of the definitions that are
 * defined in the builtins. This greatly simplifies the need to carry around huge protobuf
 * strings in tests and other places, replacing it with a simple one line function call.
 *
 * @return StatusOr<std::unique_ptr<planner::RegistryInfo>>  a pointer to the resulting registry
 * info.
 */
StatusOr<std::unique_ptr<planner::RegistryInfo>> ExportUDFInfo();

/**
 * @brief ExportUDFDocs loads all the udfs into the registry and exports the docs.
 *
 * @return udfspb::Docs
 */
udfspb::Docs ExportUDFDocs();

}  // namespace udfexporter
}  // namespace carnot
}  // namespace px
