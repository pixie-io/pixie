#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/compiler_state/registry_info.h"
#include "src/carnot/planner/ir/ir_nodes.h"
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
    PL_RETURN_IF_ERROR(analyzer->Init());
    return analyzer;
  }

 private:
  explicit Analyzer(CompilerState* compiler_state) : compiler_state_(compiler_state) {}

  void CreateSourceAndMetadataResolutionBatch() {
    RuleBatch* source_and_metadata_resolution_batch =
        CreateRuleBatch<FailOnMax>("TableAndMetadataResolution", 2);
    source_and_metadata_resolution_batch->AddRule<SourceRelationRule>(compiler_state_);
    source_and_metadata_resolution_batch->AddRule<ResolveMetadataPropertyRule>(compiler_state_,
                                                                               md_handler_.get());
    source_and_metadata_resolution_batch->AddRule<SetupJoinTypeRule>();
    source_and_metadata_resolution_batch->AddRule<MergeGroupByIntoGroupAcceptorRule>(
        IRNodeType::kBlockingAgg);
    source_and_metadata_resolution_batch->AddRule<MergeGroupByIntoGroupAcceptorRule>(
        IRNodeType::kRolling);
    source_and_metadata_resolution_batch->AddRule<ConvertStringTimesRule>(compiler_state_);
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

  void CreateOperatorCompileTimeExpressionRuleBatch() {
    RuleBatch* intermediate_resolution_batch =
        CreateRuleBatch<FailOnMax>("IntermediateResolution", 100);
    intermediate_resolution_batch->AddRule<SetMemSourceNsTimesRule>();
  }

  // TODO(philkuz) need to add a new optimization that combines maps.
  void CreateCombineConsecutiveMapsRule() {
    RuleBatch* consecutive_maps = CreateRuleBatch<FailOnMax>("CombineConsecutiveMapsRule", 2);
    consecutive_maps->AddRule<CombineConsecutiveMapsRule>();
  }

  void CreateDataTypeResolutionBatch() {
    RuleBatch* intermediate_resolution_batch =
        CreateRuleBatch<FailOnMax>("IntermediateResolution", 100);
    intermediate_resolution_batch->AddRule<DataTypeRule>(compiler_state_);
    intermediate_resolution_batch->AddRule<OperatorRelationRule>(compiler_state_);
    intermediate_resolution_batch->AddRule<DropToMapOperatorRule>(compiler_state_);
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

  void CreateTypeResolutionBatch() {
    RuleBatch* type_resolution_batch = CreateRuleBatch<FailOnMax>("TypeResolution", 2);
    type_resolution_batch->AddRule<ResolveTypesRule>(compiler_state_);
  }

  Status Init() {
    md_handler_ = MetadataHandler::Create();
    CreateSourceAndMetadataResolutionBatch();
    CreateUniqueSinkNamesBatch();
    CreateAddLimitToBatchResultSinkBatch();
    CreateOperatorCompileTimeExpressionRuleBatch();
    CreateCombineConsecutiveMapsRule();
    CreateDataTypeResolutionBatch();
    CreateMetadataConversionBatch();
    CreateResolutionVerificationBatch();
    CreateRemoveIROnlyNodesBatch();
    // TODO(james, PP-2065): once the old relation rules are removed this will probably have to move
    // closer to the top.
    CreateTypeResolutionBatch();
    return Status::OK();
  }

  CompilerState* compiler_state_;
  std::unique_ptr<MetadataHandler> md_handler_;
};
}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
