#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/registry_info.h"
#include "src/carnot/compiler/rule_executor.h"
#include "src/carnot/compiler/rules.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace compiler {

class Analyzer : public RuleExecutor {
 private:
  explicit Analyzer(CompilerState* compiler_state) : compiler_state_(compiler_state) {}
  Status Init() {
    RuleBatch* rule_batch0 = CreateRuleBatch<FailOnMax>("TableResolution", 2);
    rule_batch0->AddRule<SourceRelationRule>(compiler_state_);
    RuleBatch* rule_batch1 = CreateRuleBatch<FailOnMax>("IntermediateResolution", 100);
    rule_batch1->AddRule<DataTypeRule>(compiler_state_);
    rule_batch1->AddRule<OperatorRelationRule>(compiler_state_);
    rule_batch1->AddRule<RangeArgExpressionRule>(compiler_state_);
    RuleBatch* rule_batch3 = CreateRuleBatch<FailOnMax>("Verification", 1);
    rule_batch3->AddRule<VerifyFilterExpressionRule>(compiler_state_);
    return Status::OK();
  }

 public:
  static StatusOr<std::unique_ptr<Analyzer>> Create(CompilerState* compiler_state) {
    std::unique_ptr<Analyzer> analyzer(new Analyzer(compiler_state));
    PL_RETURN_IF_ERROR(analyzer->Init());
    return analyzer;
  }

 private:
  CompilerState* compiler_state_;
};
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
