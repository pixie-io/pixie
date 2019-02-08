#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/expression_evaluator.h"
#include "src/carnot/plan/operators.h"

namespace pl {
namespace carnot {
namespace exec {

class BlockingAggNode : public ProcessingNode {
 public:
  BlockingAggNode() : ProcessingNode() {}

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator &plan_node, const RowDescriptor &output_descriptor,
                  const std::vector<RowDescriptor> &input_descriptors) override;
  Status PrepareImpl(ExecState *exec_state) override;
  Status OpenImpl(ExecState *exec_state) override;
  Status CloseImpl(ExecState *exec_state) override;
  Status ConsumeNextImpl(ExecState *exec_state, const RowBatch &rb) override;

 private:
  bool HasNoGroups() const { return plan_node_->groups().empty(); }

  struct UDAInfo {
    UDAInfo(std::unique_ptr<udf::UDA> uda_inst, udf::UDADefinition *def_ptr)
        : uda(std::move(uda_inst)), def(def_ptr) {}
    std::unique_ptr<udf::UDA> uda;
    // unowned pointer to the definition;
    udf::UDADefinition *def = nullptr;
  };

  Status EvaluateSingleExpressionNoGroups(ExecState *exec_state, const UDAInfo &uda_info,
                                          plan::AggregateExpression *expr, const RowBatch &rb);
  StatusOr<udf::UDFDataType> GetTypeOfDep(const plan::ScalarExpression &expr) const;
  std::unique_ptr<plan::BlockingAggregateOperator> plan_node_;
  std::unique_ptr<RowDescriptor> output_descriptor_;
  std::unique_ptr<RowDescriptor> input_descriptor_;

  std::vector<UDAInfo> udas_no_groups_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
