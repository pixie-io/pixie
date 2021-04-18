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

#include "src/vizier/funcs/funcs.h"
#include "src/carnot/funcs/funcs.h"
#include "src/vizier/funcs/internal/internal_impl.h"
#include "src/vizier/funcs/md_udtfs/md_udtfs.h"
namespace px {
namespace vizier {
namespace funcs {

void RegisterFuncsOrDie(const VizierFuncFactoryContext& ctx, carnot::udf::Registry* registry) {
  // All used functions must be registered here.
  ::px::carnot::funcs::RegisterFuncsOrDie(registry);
  md::RegisterFuncsOrDie(ctx, registry);
  internal::RegisterFuncsOrDie(registry);
}

}  // namespace funcs
}  // namespace vizier
}  // namespace px
