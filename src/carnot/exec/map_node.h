#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/expression_evaluator.h"
#include "src/carnot/schema/row_batch.h"
#include "src/carnot/schema/row_descriptor.h"

namespace pl {
namespace carnot {
namespace exec {

class MapNode : public ProcessingNode {
 public:
  MapNode() = default;

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator &plan_node, const schema::RowDescriptor &output_descriptor,
                  const std::vector<schema::RowDescriptor> &input_descriptors) override;
  Status PrepareImpl(ExecState *exec_state) override;
  Status OpenImpl(ExecState *exec_state) override;
  Status CloseImpl(ExecState *exec_state) override;
  Status ConsumeNextImpl(ExecState *exec_state, const schema::RowBatch &rb) override;

 private:
  std::unique_ptr<ExpressionEvaluator> evaluator_;
  std::unique_ptr<plan::MapOperator> plan_node_;
  std::unique_ptr<schema::RowDescriptor> output_descriptor_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
