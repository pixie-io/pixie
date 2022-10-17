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

#include "src/carnot/planner/compiler/compiler.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/distributed/distributed_plan/distributed_plan.h"
#include "src/carnot/planner/distributed/distributed_planner.h"
#include "src/carnot/planner/plannerpb/service.pb.h"
#include "src/carnot/planner/probes/probes.h"
#include "src/shared/scriptspb/scripts.pb.h"

#include "src/carnot/docspb/docs.pb.h"

namespace px {
namespace carnot {
namespace planner {
namespace docs {

struct DocHolder {
  std::string doc;
  std::string name;
  std::string objtype;
  absl::flat_hash_map<std::string, DocHolder> attrs;

  Status ToProto(docspb::DocstringNode* pb) const;
};

struct DocExtractor {
  DocHolder ExtractDoc(const compiler::QLObjectPtr& qobject);
};

}  // namespace docs
}  // namespace planner
}  // namespace carnot
}  // namespace px
