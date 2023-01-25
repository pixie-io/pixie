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

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/column_ir.h"
#include "src/carnot/planner/ir/expression_ir.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/metadatapb/metadata.pb.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

class GroupAcceptorIR : public OperatorIR {
 public:
  using OperatorIR::OperatorIR;
  std::vector<ColumnIR*> groups() const { return groups_; }
  bool group_by_all() const { return groups_.size() == 0; }

  Status SetGroups(const std::vector<ColumnIR*>& new_groups) {
    DCHECK(groups_.empty());
    groups_.resize(new_groups.size());
    for (size_t i = 0; i < new_groups.size(); ++i) {
      PX_ASSIGN_OR_RETURN(groups_[i], graph()->OptionallyCloneWithEdge(this, new_groups[i]));
    }
    return Status::OK();
  }

 private:
  std::vector<ColumnIR*> groups_;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
