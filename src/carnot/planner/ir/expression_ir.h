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

#include "src/carnot/planner/compiler_error_context/compiler_error_context.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/compilerpb/compiler_status.pb.h"
#include "src/carnot/planner/ir/ir_node.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/shared/metadatapb/metadata.pb.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {

// Forward declaration for types that are used in ExpressionIR. They are declared later in this
// header.
class ColumnIR;

class ExpressionIR : public IRNode {
 public:
  ExpressionIR() = delete;

  bool IsOperator() const override { return false; }
  bool IsExpression() const override { return true; }
  virtual types::DataType EvaluatedDataType() const = 0;
  virtual bool IsDataTypeEvaluated() const = 0;
  virtual bool IsColumn() const { return false; }
  virtual bool IsData() const { return false; }
  virtual bool IsFunction() const { return false; }
  virtual bool Equals(ExpressionIR* expr) const = 0;
  virtual Status ToProto(planpb::ScalarExpression* expr) const = 0;
  static bool NodeMatches(IRNode* input);
  static std::string class_type_string() { return "Expression"; }
  StatusOr<absl::flat_hash_set<ColumnIR*>> InputColumns();
  StatusOr<absl::flat_hash_set<std::string>> InputColumnNames();
  static constexpr bool FailOnResolveType() { return false; }

  bool HasTypeCast() const { return type_cast_ != nullptr; }
  std::shared_ptr<ValueType> type_cast() const { return type_cast_; }
  void SetTypeCast(std::shared_ptr<ValueType> type_cast) { type_cast_ = type_cast; }

  /**
   * @brief Override of CopyFromNode that adds special handling for ExpressionIR.
   */
  Status CopyFromNode(const IRNode* node,
                      absl::flat_hash_map<const IRNode*, IRNode*>* copied_nodes_map) override;

  using MetadataType = shared::metadatapb::MetadataType;

  /**
   * @brief Annotations describing a particular expression. For example, labeling it
   * as representing a metadata field, such as pod_name.
   */
  struct Annotations {
    MetadataType metadata_type = MetadataType::METADATA_TYPE_UNKNOWN;
    bool metadata_type_set() const { return metadata_type != MetadataType::METADATA_TYPE_UNKNOWN; }
    void clear_metadata_type() { metadata_type = MetadataType::METADATA_TYPE_UNKNOWN; }

    Annotations() {}
    explicit Annotations(MetadataType md_type) : metadata_type(md_type) {}

    bool operator==(const Annotations& other) const { return metadata_type == other.metadata_type; }
    bool operator!=(const Annotations& other) const { return !(*this == other); }
    /**
     * @brief Compute the shared overlap between two Annotations.
     */
    static Annotations Intersection(const Annotations& lhs, const Annotations& rhs) {
      Annotations output;
      if (lhs.metadata_type == rhs.metadata_type) {
        output.metadata_type = lhs.metadata_type;
      }
      return output;
    }

    /**
     * @brief Union two Annotations together. When the fields are both set and differ,
     * defaults to the value specified by lhs.
     */
    static Annotations Union(const Annotations& lhs, const Annotations& rhs) {
      Annotations output = lhs;
      if (!output.metadata_type_set()) {
        output.metadata_type = rhs.metadata_type;
      }
      return output;
    }
  };

  const Annotations& annotations() const { return annotations_; }
  void set_annotations(const Annotations& annotations) { annotations_ = annotations; }

  ValueTypePtr resolved_value_type() const {
    return std::static_pointer_cast<ValueType>(resolved_type());
  }

 protected:
  ExpressionIR(int64_t id, IRNodeType type, const Annotations& annotations)
      : IRNode(id, type), annotations_(annotations) {}
  Annotations annotations_;
  std::shared_ptr<ValueType> type_cast_ = nullptr;
};
}  // namespace planner
}  // namespace carnot
}  // namespace px
