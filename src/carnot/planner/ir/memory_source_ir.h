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
#include "src/carnot/planner/ir/expression_ir.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

/**
 * @brief The MemorySourceIR is a dual logical plan
 * and IR node operator. It inherits from both classes
 */
class MemorySourceIR : public OperatorIR {
 public:
  MemorySourceIR() = delete;
  explicit MemorySourceIR(int64_t id) : OperatorIR(id, IRNodeType::kMemorySource) {}

  /**
   * @brief Initialize the memory source.
   *
   * @param table_name the table to load.
   * @param select_columns the columns to select. If vector is empty, then select all columns.
   * @return Status
   */
  Status Init(const std::string& table_name, const std::vector<std::string>& select_columns);

  std::string table_name() const { return table_name_; }

  // Whether or not the MemorySource should be executed in persistent streaming mode,
  // where the MemorySource reads data indefinitely.
  bool streaming() const { return streaming_; }
  void set_streaming(bool streaming) { streaming_ = streaming; }

  void SetTimeStartNS(int64_t time_start_ns) { time_start_ns_ = time_start_ns; }
  void SetTimeStopNS(int64_t time_stop_ns) { time_stop_ns_ = time_stop_ns; }
  bool IsTimeStartSet() const { return time_start_ns_.has_value(); }
  bool IsTimeStopSet() const { return time_stop_ns_.has_value(); }

  std::string DebugString() const override;

  int64_t time_start_ns() const { return time_start_ns_.value(); }
  int64_t time_stop_ns() const { return time_stop_ns_.value(); }

  const std::vector<int64_t>& column_index_map() const { return column_index_map_; }
  bool column_index_map_set() const { return column_index_map_set_; }
  void SetColumnIndexMap(const std::vector<int64_t>& column_index_map) {
    column_index_map_set_ = true;
    column_index_map_ = column_index_map;
  }

  Status ToProto(planpb::Operator*) const override;

  bool select_all() const { return column_names_.size() == 0; }

  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;
  const std::vector<std::string>& column_names() const { return column_names_; }
  void SetTabletValue(const types::TabletID& tablet_value) {
    tablet_value_ = tablet_value;
    has_tablet_value_ = true;
  }
  bool HasTablet() const { return has_tablet_value_; }

  const types::TabletID& tablet_value() const {
    DCHECK(HasTablet());
    return tablet_value_;
  }

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override {
    return std::vector<absl::flat_hash_set<std::string>>{};
  }

  void SetColumnNames(const std::vector<std::string>& col_names) { column_names_ = col_names; }

  bool IsSource() const override { return true; }

  Status ResolveType(CompilerState* compiler_state);

 protected:
  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& output_colnames) override;

 private:
  std::string table_name_;
  bool streaming_ = false;

  std::optional<int64_t> time_start_ns_;
  std::optional<int64_t> time_stop_ns_;

  // Hold of columns in the order that they are selected.
  std::vector<std::string> column_names_;

  // The mapping of the source's column indices to the current columns, as given by column_names_.
  std::vector<int64_t> column_index_map_;
  bool column_index_map_set_ = false;

  types::TabletID tablet_value_;
  bool has_tablet_value_ = false;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
