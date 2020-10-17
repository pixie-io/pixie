#pragma once

#include <stddef.h>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/expression_evaluator.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/udf/base.h"
#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace exec {

class FilterNode : public ProcessingNode {
 public:
  FilterNode() = default;
  virtual ~FilterNode() = default;

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator& plan_node) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status ConsumeNextImpl(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                         size_t parent_index) override;

 private:
  std::unique_ptr<VectorNativeScalarExpressionEvaluator> evaluator_;
  std::unique_ptr<plan::FilterOperator> plan_node_;
  std::unique_ptr<udf::FunctionContext> function_ctx_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
