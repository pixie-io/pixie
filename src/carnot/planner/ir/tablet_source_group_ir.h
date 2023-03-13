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
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

/*
 * @brief TabletSourceGroup should is the container for Tablets in the system.
 * It is a temporary representation that can then be used to convert a previous Memory
 * Source into a Union of sources with tablet key values.
 *
 */
class TabletSourceGroupIR : public OperatorIR {
 public:
  TabletSourceGroupIR() = delete;

  Status Init(MemorySourceIR* memory_source_ir, const std::vector<types::TabletID>& tablets,
              const std::string& tablet_key) {
    tablets_ = tablets;
    memory_source_ir_ = memory_source_ir;
    DCHECK(memory_source_ir->is_type_resolved());
    PX_RETURN_IF_ERROR(SetResolvedType(memory_source_ir->resolved_type()));
    DCHECK(resolved_table_type()->HasColumn(tablet_key));
    tablet_key_ = tablet_key;
    return Status::OK();
  }

  explicit TabletSourceGroupIR(int64_t id) : OperatorIR(id, IRNodeType::kTabletSourceGroup) {}

  Status ToProto(planpb::Operator*) const override {
    return error::Unimplemented("$0::ToProto not implemented because no use found for it yet.",
                                DebugString());
  }
  Status CopyFromNodeImpl(const IRNode*, absl::flat_hash_map<const IRNode*, IRNode*>*) override {
    return error::Unimplemented("$0::CopyFromNode not implemented because no use found for it yet.",
                                DebugString());
  }

  const std::vector<types::TabletID>& tablets() const { return tablets_; }
  /**
   * @brief Returns the Memory source that was replaced by this node.
   * @return MemorySourceIR*
   */
  MemorySourceIR* ReplacedMemorySource() const { return memory_source_ir_; }

  const std::string tablet_key() const { return tablet_key_; }

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override {
    return error::Unimplemented("Unexpected call to TabletSourceGroupIR::RequiredInputColumns");
  }

  bool IsBlocking() const override { return false; }
  bool IsSource() const override { return true; }

  static constexpr bool FailOnResolveType() { return true; }

 protected:
  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& /*kept_columns*/) override {
    return error::Unimplemented("Unexpected call to TabletSourceGroupIR::PruneOutputColumnsTo.");
  }

 private:
  // The key in the table that is used as a tablet_key.
  std::string tablet_key_;
  // The tablets that are associated with this node.
  std::vector<types::TabletID> tablets_;
  // The memory source that this node replaces. Deleted from the graph when this node is deleted.
  MemorySourceIR* memory_source_ir_;
};
}  // namespace planner
}  // namespace carnot
}  // namespace px
