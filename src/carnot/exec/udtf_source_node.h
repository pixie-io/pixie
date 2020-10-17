#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/udf/base.h"
#include "src/carnot/udf/udf.h"
#include "src/carnot/udf/udf_definition.h"
#include "src/carnot/udf/udtf.h"
#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/table_store/schema/row_descriptor.h"

namespace pl {
namespace carnot {
namespace exec {

class UDTFSourceNode : public SourceNode {
 public:
  UDTFSourceNode() = default;
  virtual ~UDTFSourceNode() = default;

  bool NextBatchReady() override;

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator& plan_node) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status GenerateNextImpl(ExecState* exec_state) override;

 private:
  bool has_more_batches_ = true;
  udf::UDTFDefinition* udtf_def_ = nullptr;
  std::unique_ptr<plan::UDTFSourceOperator> plan_node_;
  std::unique_ptr<udf::FunctionContext> function_ctx_;
  std::unique_ptr<udf::AnyUDTF> udtf_inst_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
