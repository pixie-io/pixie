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
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/udfspb/udfs.pb.h"
#include "src/common/base/base.h"
#include "src/shared/metadatapb/metadata.pb.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

class MetadataProperty : public NotCopyable {
 public:
  MetadataProperty() = delete;
  virtual ~MetadataProperty() = default;

  /**
    @brief Returns a bool that notifies whether an expression fits the expected format for this
   * property when comparing.  This is used to make sure comparison operations (==, >, <, !=) are
   * pre-checked during compilation, preventing unnecssary operations during execution and exposing
   * query errors to the user.
   *
   * For example, we expect values compared to POD_NAMES to be Strings of the format
   * `<namespace>/<pod-name>`.
   *
   * ExplainFormat should describe the expected format of this method in string form.
   *
   * @param value: the ExpressionIR node that
   */
  virtual bool ExprFitsFormat(ExpressionIR* value) const = 0;

  /**
   * @brief Describes the Expression format that ExprFitsFormat expects.
   * This will be passed up to query writing users so you should prioritize pythonic descriptions
   * over internal compiler jargon.
   */
  virtual std::string ExplainFormat() const = 0;

  /**
   * @brief Returns the key columns formatted as metadata columns.
   */
  std::vector<std::string> GetKeyColumnReprs() const {
    std::vector<std::string> columns;
    for (const auto& c : key_columns_) {
      columns.push_back(GetMetadataString(c));
    }
    return columns;
  }

  /**
   * @brief Returns whether this metadata key can be obtained by the provided key column.
   * @param key: the column name string as seen in Carnot.
   */
  inline bool HasKeyColumn(const std::string_view key) {
    auto columns = GetKeyColumnReprs();
    return std::find(columns.begin(), columns.end(), key) != columns.end();
  }

  /**
   * @brief Returns the udf-string that converts a given key column to the Metadata
   * represented by this property.
   */
  StatusOr<std::string> UDFName(const std::string_view key) {
    if (!HasKeyColumn(key)) {
      return error::InvalidArgument(
          "Key column $0 invalid for metadata value $1. Expected one of [$2].", key, name_,
          absl::StrJoin(key_columns_, ","));
    }
    return absl::Substitute("$1_to_$0", name_, key);
  }

  // Getters.
  inline std::string name() const { return name_; }
  inline shared::metadatapb::MetadataType metadata_type() const { return metadata_type_; }
  inline types::DataType column_type() const { return column_type_; }

  inline static std::string GetMetadataString(shared::metadatapb::MetadataType metadata_type) {
    std::string name = shared::metadatapb::MetadataType_Name(metadata_type);
    absl::AsciiStrToLower(&name);
    return name;
  }

 protected:
  MetadataProperty(shared::metadatapb::MetadataType metadata_type, types::DataType column_type,
                   std::vector<shared::metadatapb::MetadataType> key_columns)
      : metadata_type_(metadata_type), column_type_(column_type), key_columns_(key_columns) {
    name_ = GetMetadataString(metadata_type);
  }

 private:
  shared::metadatapb::MetadataType metadata_type_;
  types::DataType column_type_;
  std::string name_;
  std::vector<shared::metadatapb::MetadataType> key_columns_;
};

class MetadataIR : public ColumnIR {
 public:
  MetadataIR() = delete;
  explicit MetadataIR(int64_t id) : ColumnIR(id, IRNodeType::kMetadata) {}
  Status Init(const std::string& metadata_val, int64_t parent_op_idx);

  std::string name() const { return metadata_name_; }
  MetadataProperty* property() const { return property_; }
  bool has_property() const { return property_ != nullptr; }
  void set_property(MetadataProperty* property) { property_ = property; }

  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;

  std::string DebugString() const override;
  Status ResolveType(CompilerState* compiler_state, const std::vector<TypePtr>& parent_types);

 private:
  std::string metadata_name_;
  MetadataProperty* property_ = nullptr;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
