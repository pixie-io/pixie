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
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/metadatapb/metadata.pb.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

/**
 * The MemorySinkIR describes the MemorySink operator.
 */
class MemorySinkIR : public OperatorIR {
 public:
  MemorySinkIR() = delete;
  explicit MemorySinkIR(int64_t id) : OperatorIR(id, IRNodeType::kMemorySink) {}

  std::string name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }
  Status ToProto(planpb::Operator*) const override;

  Status Init(OperatorIR* parent, const std::string& name,
              const std::vector<std::string> out_columns);

  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;
  // When out_columns_ is empty, the full input relation will be written to the sink.
  const std::vector<std::string>& out_columns() const { return out_columns_; }
  Status SetOutColumns(std::vector<std::string> new_out_columns) {
    out_columns_ = new_out_columns;
    return Status::OK();
  }
  bool IsBlocking() const override { return true; }

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override {
    DCHECK(is_type_resolved());
    auto out_cols = resolved_table_type()->ColumnNames();
    absl::flat_hash_set<std::string> outputs{out_cols.begin(), out_cols.end()};
    return std::vector<absl::flat_hash_set<std::string>>{outputs};
  }

  Status ResolveType(CompilerState* compiler_state);

 protected:
  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& /*output_colnames*/) override {
    // This shouldn't occur, because MemorySinkIR has no parents.
    return error::Unimplemented("Unexpected call to MemorySinkIR::PruneOutputColumnsTo.");
  }

 private:
  std::string name_;
  std::vector<std::string> out_columns_;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
