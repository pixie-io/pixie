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

#include <absl/container/flat_hash_set.h>
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/column_expression.h"
#include "src/carnot/planner/ir/column_ir.h"
#include "src/carnot/planner/ir/expression_ir.h"
#include "src/carnot/planner/ir/group_acceptor_ir.h"
#include "src/carnot/planner/ir/operator_ir.h"
#include "src/carnot/planner/ir/otel_export_sink_ir.h"
#include "src/carnot/planner/ir/string_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"
#include "src/shared/metadatapb/metadata.pb.h"
#include "src/shared/types/types.h"
#include "src/shared/types/typespb/types.pb.h"

namespace px {
namespace carnot {
namespace planner {
std::string ConvertSemanticTypeToOtel(const types::SemanticType& stype);

struct OTelAttribute {
  // The name of the attribute
  std::string name;
  // The column that references this attribute.
  ColumnIR* column_reference = nullptr;
  // The string value to use for the attribute value.
  // Only used if column_reference is null.
  std::string string_value;

  Status ToProto(planpb::OTelAttribute* attribute) const {
    attribute->set_name(name);
    if (column_reference == nullptr) {
      attribute->set_string_value(string_value);
      return Status::OK();
    }
    auto column_pb = attribute->mutable_column();

    auto column_type = column_reference->resolved_value_type();
    DCHECK(column_type);
    auto column_data_type = column_type->data_type();

    if (column_data_type != types::STRING && column_data_type != types::INT64 &&
        column_data_type != types::FLOAT64 && column_data_type != types::BOOLEAN) {
      return column_reference->CreateIRNodeError(
          "Expected attribute column '$0' to be STRING, INT64, FLOAT64, or BOOLEAN, received $1",
          column_reference->col_name(), types::ToString(column_data_type));
    }

    if (column_type->semantic_type() == types::ST_SERVICE_NAME) {
      column_pb->set_can_be_json_encoded_array(true);
    }

    column_pb->set_column_type(column_data_type);
    PX_ASSIGN_OR_RETURN(auto index, column_reference->GetColumnIndex());
    column_pb->set_column_index(index);
    return Status::OK();
  }
};

struct OTelMetricGauge {
  ColumnIR* value_column;
};

struct OTelMetricSummary {
  struct QuantileValues {
    double quantile;
    ColumnIR* value_column;
  };
  ColumnIR* count_column;
  ColumnIR* sum_column;
  std::vector<QuantileValues> quantiles;
};

struct OTelMetric {
  std::string name;
  std::string description;

  // Unit string is the string representing the unit. If it's not set, the compiler will attempt
  // to determine the unit from the unit_column semantic type.
  std::string unit_str;
  // The column where we get the unit value from.
  ColumnIR* unit_column;

  ColumnIR* time_column;

  std::vector<OTelAttribute> attributes;

  std::variant<OTelMetricGauge, OTelMetricSummary> metric;
};

struct OTelSpan {
  std::variant<std::string, ColumnIR*> name;
  std::vector<OTelAttribute> attributes;

  ColumnIR* trace_id_column = nullptr;
  ColumnIR* span_id_column = nullptr;
  ColumnIR* parent_span_id_column = nullptr;

  ColumnIR* start_time_column;
  ColumnIR* end_time_column;

  int64_t span_kind;
};

struct OTelData {
  planpb::OTelEndpointConfig endpoint_config;
  std::vector<OTelAttribute> resource_attributes;
  std::vector<OTelMetric> metrics;
  std::vector<OTelSpan> spans;
};

/**
 * @brief The IR representation for the OTelExportSink operator.
 * Represents a configuration to transform a DataFrame into OpenTelemetry
 * data.
 */
class OTelExportSinkIR : public OperatorIR {
 public:
  explicit OTelExportSinkIR(int64_t id) : OperatorIR(id, IRNodeType::kOTelExportSink) {}

  Status Init(OperatorIR* parent, const OTelData& data) {
    PX_RETURN_IF_ERROR(ProcessConfig(data));
    return AddParent(parent);
  }

  Status ToProto(planpb::Operator* op) const override;

  StatusOr<ColumnIR*> AddColumn(ColumnIR* column) {
    if (column == nullptr) {
      return CreateIRNodeError("column not defined");
    }
    PX_ASSIGN_OR_RETURN(auto copied_column, graph()->CopyNode(column));
    required_column_names_.insert(copied_column->col_name());
    PX_RETURN_IF_ERROR(graph()->AddEdge(this, copied_column));
    columns_to_resolve_.push_back(copied_column);
    return copied_column;
  }

  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>*) override;

  Status ResolveType(CompilerState* compiler_state);
  inline bool IsBlocking() const override { return true; }

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override;

 protected:
  Status ProcessConfig(const OTelData& data);

  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& /*kept_columns*/) override {
    return error::Unimplemented("Unexpected call to OTelExportSinkIR::PruneOutputColumnsTo.");
  }

 private:
  OTelData data_;
  absl::flat_hash_set<std::string> required_column_names_;
  std::vector<ColumnIR*> columns_to_resolve_;
};
}  // namespace planner
}  // namespace carnot
}  // namespace px
