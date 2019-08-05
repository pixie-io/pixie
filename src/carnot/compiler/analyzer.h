#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/metadata_handler.h"
#include "src/carnot/compiler/registry_info.h"
#include "src/carnot/compiler/rule_executor.h"
#include "src/carnot/compiler/rules.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace compiler {

class Analyzer : public RuleExecutor {
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
    source_and_metadata_resolution_batch->AddRule<ResolveMetadataRule>(compiler_state_,
                                                                       md_handler_.get());
    source_and_metadata_resolution_batch->AddRule<MetadataFunctionFormatRule>(compiler_state_);
  }

  void CreateVerifyUserDefinedColumnsBatch() {
    RuleBatch* verify_user_columns_batch =
        CreateRuleBatch<FailOnMax>("VerifyUserDefinedColumns", 1);
    verify_user_columns_batch->AddRule<CheckMetadataColumnNamingRule>(compiler_state_);
  }

  void CreateDataTypeResolutionBatch() {
    RuleBatch* intermediate_resolution_batch =
        CreateRuleBatch<FailOnMax>("IntermediateResolution", 100);
    intermediate_resolution_batch->AddRule<DataTypeRule>(compiler_state_);
    intermediate_resolution_batch->AddRule<OperatorRelationRule>(compiler_state_);
    intermediate_resolution_batch->AddRule<RangeArgExpressionRule>(compiler_state_);
  }

  void CreateResolutionVerificationBatch() {
    RuleBatch* resolution_verification_batch =
        CreateRuleBatch<FailOnMax>("ResolutionVerification", 1);
    resolution_verification_batch->AddRule<VerifyFilterExpressionRule>(compiler_state_);
  }

  void CreateRemoveIROnlyNodesBatch() {
    RuleBatch* remove_ir_only_nodes_batch = CreateRuleBatch<FailOnMax>("RemoveIROnlyNodes", 2);
    remove_ir_only_nodes_batch->AddRule<MetadataResolverConversionRule>(compiler_state_);
    remove_ir_only_nodes_batch->AddRule<MergeRangeOperatorRule>(compiler_state_);
  }

  Status Init() {
    md_handler_ = MetadataHandler::Create();
    CreateSourceAndMetadataResolutionBatch();
    CreateVerifyUserDefinedColumnsBatch();
    CreateDataTypeResolutionBatch();
    CreateResolutionVerificationBatch();
    CreateRemoveIROnlyNodesBatch();
    return Status::OK();
  }

  CompilerState* compiler_state_;
  std::unique_ptr<MetadataHandler> md_handler_;
};
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
