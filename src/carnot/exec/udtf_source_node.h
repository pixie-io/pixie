#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/udf/udf.h"

namespace pl {
namespace carnot {
namespace exec {

class UDTFSourceNode : public SourceNode {
 public:
  UDTFSourceNode() = default;
  bool HasBatchesRemaining() override;
  bool NextBatchReady() override;

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(
      const plan::Operator& plan_node, const table_store::schema::RowDescriptor& output_descriptor,
      const std::vector<table_store::schema::RowDescriptor>& input_descriptors) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status GenerateNextImpl(ExecState* exec_state) override;

 private:
  bool has_more_batches_ = true;
  udf::UDTFDefinition* udtf_def_ = nullptr;
  std::unique_ptr<plan::UDTFSourceOperator> plan_node_;
  std::unique_ptr<table_store::schema::RowDescriptor> output_descriptor_;
  std::unique_ptr<udf::FunctionContext> function_ctx_;
  std::unique_ptr<udf::AnyUDTF> udtf_inst_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
