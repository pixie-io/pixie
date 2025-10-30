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
#include "src/common/base/base.h"
#include "src/shared/types/types.h"
#include "src/table_store/schema/relation.h"

namespace px {
namespace carnot {
namespace planner {

/**
 * @brief The ClickHouseSourceIR represents a source that reads data from a ClickHouse database.
 */
class ClickHouseSourceIR : public OperatorIR {
 public:
  ClickHouseSourceIR() = delete;
  explicit ClickHouseSourceIR(int64_t id) : OperatorIR(id, IRNodeType::kClickHouseSource) {}

  /**
   * @brief Initialize the ClickHouse source.
   *
   * @param table_name the table to load.
   * @param select_columns the columns to select. If vector is empty, then select all columns.
   * @return Status
   */
  Status Init(const std::string& table_name, const std::vector<std::string>& select_columns);

  std::string table_name() const { return table_name_; }

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

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override {
    return std::vector<absl::flat_hash_set<std::string>>{};
  }

  void SetColumnNames(const std::vector<std::string>& col_names) { column_names_ = col_names; }

  bool IsSource() const override { return true; }

  Status ResolveType(CompilerState* compiler_state);

 protected:
  // Helper method to query ClickHouse for table schema and create a Relation
  StatusOr<table_store::schema::Relation> InferRelationFromClickHouse(
      CompilerState* compiler_state, const std::string& table_name);

  // Helper method to convert ClickHouse type string to Pixie DataType
  static StatusOr<types::DataType> ClickHouseTypeToPixieType(const std::string& ch_type_name);

  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& output_colnames) override;

 private:
  std::string table_name_;

  std::optional<int64_t> time_start_ns_;
  std::optional<int64_t> time_stop_ns_;

  // Hold of columns in the order that they are selected.
  std::vector<std::string> column_names_;

  // The mapping of the source's column indices to the current columns, as given by column_names_.
  std::vector<int64_t> column_index_map_;
  bool column_index_map_set_ = false;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
