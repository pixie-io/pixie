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
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/carnot/planner/compiler/analyzer/add_limit_to_batch_result_sink_rule.h"
#include "src/carnot/planner/compiler/analyzer/combine_consecutive_maps_rule.h"
#include "src/carnot/planner/compiler/analyzer/convert_metadata_rule.h"
#include "src/carnot/planner/compiler/analyzer/drop_to_map_rule.h"
#include "src/carnot/planner/compiler/analyzer/merge_group_by_into_group_acceptor_rule.h"
#include "src/carnot/planner/compiler/analyzer/nested_blocking_agg_fn_check_rule.h"
#include "src/carnot/planner/compiler/analyzer/propagate_expression_annotations_rule.h"
#include "src/carnot/planner/compiler/analyzer/remove_group_by_rule.h"
#include "src/carnot/planner/compiler/analyzer/resolve_metadata_property_rule.h"
#include "src/carnot/planner/compiler/analyzer/resolve_stream_rule.h"
#include "src/carnot/planner/compiler/analyzer/resolve_types_rule.h"
#include "src/carnot/planner/compiler/analyzer/restrict_columns_rule.h"
#include "src/carnot/planner/compiler/analyzer/setup_join_type_rule.h"
#include "src/carnot/planner/compiler/analyzer/unique_sink_names_rule.h"
#include "src/carnot/planner/compiler/analyzer/verify_filter_expression_rule.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/compiler_state/registry_info.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/rules/rule_executor.h"
#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

class Analyzer : public RuleExecutor<IR> {
 public:
  static StatusOr<std::unique_ptr<Analyzer>> Create(CompilerState* compiler_state) {
    std::unique_ptr<Analyzer> analyzer(new Analyzer(compiler_state));
    PX_RETURN_IF_ERROR(analyzer->Init());
    return analyzer;
  }

 private:
  explicit Analyzer(CompilerState* compiler_state) : compiler_state_(compiler_state) {}

  void CreateSourceAndMetadataResolutionBatch() {
    RuleBatch* source_and_metadata_resolution_batch =
        CreateRuleBatch<FailOnMax>("TableAndMetadataResolution", 2);
    source_and_metadata_resolution_batch->AddRule<ResolveMetadataPropertyRule>(compiler_state_,
                                                                               md_handler_.get());
    source_and_metadata_resolution_batch->AddRule<SetupJoinTypeRule>();
    source_and_metadata_resolution_batch->AddRule<MergeGroupByIntoGroupAcceptorRule>(
        IRNodeType::kBlockingAgg);
    source_and_metadata_resolution_batch->AddRule<MergeGroupByIntoGroupAcceptorRule>(
        IRNodeType::kRolling);
    source_and_metadata_resolution_batch->AddRule<NestedBlockingAggFnCheckRule>();
    source_and_metadata_resolution_batch->AddRule<ResolveStreamRule>();
  }

  void CreateUniqueSinkNamesBatch() {
    RuleBatch* unique_sink_names = CreateRuleBatch<TryUntilMax>("UniqueSinkNames", 1);
    unique_sink_names->AddRule<UniqueSinkNameRule>();
  }

  void CreateAddLimitToBatchResultSinkBatch() {
    RuleBatch* limit_to_res_sink = CreateRuleBatch<FailOnMax>("AddLimitToBatchResultSink", 2);
    limit_to_res_sink->AddRule<AddLimitToBatchResultSinkRule>(compiler_state_);
  }

  // TODO(philkuz) need to add a new optimization that combines maps.
  void CreateCombineConsecutiveMapsRule() {
    RuleBatch* consecutive_maps = CreateRuleBatch<FailOnMax>("CombineConsecutiveMapsRule", 2);
    consecutive_maps->AddRule<CombineConsecutiveMapsRule>();
  }

  void CreateDataTypeResolutionBatch() {
    RuleBatch* intermediate_resolution_batch =
        CreateRuleBatch<FailOnMax>("DataTypeResolution", 100);
    intermediate_resolution_batch->AddRule<ResolveTypesRule>(compiler_state_);
    intermediate_resolution_batch->AddRule<DropToMapOperatorRule>(compiler_state_);
  }
  void CreateManageColumnAccessBatch() {
    RuleBatch* manage_column_access = CreateRuleBatch<TryUntilMax>("ManageColumnAccess", 1);
    manage_column_access->AddRule<RestrictColumnsRule>(compiler_state_);
  }

  void CreateMetadataConversionBatch() {
    RuleBatch* metadata_conversion_batch = CreateRuleBatch<FailOnMax>("MetadataConversion", 2);
    metadata_conversion_batch->AddRule<ConvertMetadataRule>(compiler_state_);
    metadata_conversion_batch->AddRule<PropagateExpressionAnnotationsRule>();
  }

  void CreateResolutionVerificationBatch() {
    RuleBatch* resolution_verification_batch =
        CreateRuleBatch<FailOnMax>("ResolutionVerification", 1);
    resolution_verification_batch->AddRule<VerifyFilterExpressionRule>(compiler_state_);
  }

  void CreateRemoveIROnlyNodesBatch() {
    RuleBatch* remove_ir_only_nodes_batch = CreateRuleBatch<FailOnMax>("RemoveIROnlyNodes", 2);
    remove_ir_only_nodes_batch->AddRule<RemoveGroupByRule>();
  }

  Status Init() {
    md_handler_ = MetadataHandler::Create();
    CreateSourceAndMetadataResolutionBatch();
    CreateUniqueSinkNamesBatch();
    CreateAddLimitToBatchResultSinkBatch();
    CreateCombineConsecutiveMapsRule();
    CreateDataTypeResolutionBatch();
    CreateManageColumnAccessBatch();
    CreateMetadataConversionBatch();
    CreateResolutionVerificationBatch();
    CreateRemoveIROnlyNodesBatch();
    return Status::OK();
  }

  CompilerState* compiler_state_;
  std::unique_ptr<MetadataHandler> md_handler_;
};
}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
