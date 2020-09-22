#pragma once

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/plan/operators.h"
#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/table_store/schema/row_batch.h"

namespace pl {
namespace carnot {
namespace exec {

class EmptySourceNode : public SourceNode {
 public:
  EmptySourceNode() = default;
  bool NextBatchReady() override;

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator& plan_node) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status GenerateNextImpl(ExecState* exec_state) override;

 private:
  std::unique_ptr<plan::EmptySourceOperator> plan_node_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
