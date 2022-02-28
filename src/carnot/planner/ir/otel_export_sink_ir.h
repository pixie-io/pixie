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
#include "src/carnot/planner/ir/string_ir.h"
#include "src/carnot/planner/types/types.h"
#include "src/common/base/base.h"
#include "src/shared/metadatapb/metadata.pb.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace planner {
struct ExpectedColumn {
  // The node where we create the error if one exists.
  StringIR* error_node;
  // The name of the argument that points to this column
  std::string arg;
  // The name of the column referenced.
  std::string colname;
  // The set of types that are valid for this column.
  absl::flat_hash_set<types::DataType> coltypes;
};
/**
 * @brief The BlockingAggIR is the IR representation for the Agg operator.
 * GroupBy groups() and Aggregate columns according to aggregate_expressions().
 */
class OTelExportSinkIR : public OperatorIR {
 public:
  explicit OTelExportSinkIR(int64_t id) : OperatorIR(id, IRNodeType::kOTelExportSink) {}

  Status Init(OperatorIR* parent, const planpb::OTelExportSinkOperator& otel_op,
              const std::vector<ExpectedColumn>& expected_columns);

  Status ToProto(planpb::Operator*) const override;
  Status CopyFromNodeImpl(const IRNode* node,
                          absl::flat_hash_map<const IRNode*, IRNode*>*) override;

  inline bool IsBlocking() const override { return true; }

  StatusOr<std::vector<absl::flat_hash_set<std::string>>> RequiredInputColumns() const override;

  Status UpdateOpAfterParentTypesResolvedImpl() override;

 protected:
  StatusOr<absl::flat_hash_set<std::string>> PruneOutputColumnsToImpl(
      const absl::flat_hash_set<std::string>& /*kept_columns*/) override {
    return error::Unimplemented("Unexpected call to OTelExportSinkIR::PruneOutputColumnsTo.");
  }

 private:
  planpb::OTelExportSinkOperator otel_op_;
  // The columns this expects.
  std::vector<ExpectedColumn> expected_columns_;
};
}  // namespace planner
}  // namespace carnot
}  // namespace px
