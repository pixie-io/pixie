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

#include "src/carnot/funcs/funcs.h"

#include "src/carnot/funcs/builtins/builtins.h"
#include "src/carnot/funcs/metadata/metadata_ops.h"
#include "src/carnot/funcs/net/net_ops.h"
#include "src/carnot/funcs/os/process_ops.h"
#include "src/carnot/funcs/protocols/protocol_ops.h"

namespace px {
namespace carnot {
namespace funcs {

void RegisterFuncsOrDie(udf::Registry* registry) {
  builtins::RegisterBuiltinsOrDie(registry);
  metadata::RegisterMetadataOpsOrDie(registry);
  net::RegisterNetOpsOrDie(registry);
  protocols::RegisterProtocolOpsOrDie(registry);
  os::RegisterProcessOpsOrDie(registry);
}

}  // namespace funcs
}  // namespace carnot
}  // namespace px
