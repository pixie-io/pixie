#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "src/carnot/exec/source_node.h"
#include "src/carnot/proto/plan.pb.h"
namespace pl {
namespace carnot {
namespace exec {

std::string SourceNode::DebugStringImpl() { return "Exec::SourceNode"; }

Status SourceNode::InitImpl(const plan::Operator &plan_node, const RowDescriptor &output_descriptor,
                            const std::vector<RowDescriptor> &) {
  CHECK(plan_node.op_type() == carnotpb::OperatorType::MEMORY_SOURCE_OPERATOR);
  const auto *source_plan_node = static_cast<const plan::MemorySourceOperator *>(&plan_node);
  // copy the plan node to local object;
  plan_node_ = std::make_unique<plan::MemorySourceOperator>(*source_plan_node);
  output_descriptor_ = std::make_unique<RowDescriptor>(output_descriptor);

  return Status::OK();
}
Status SourceNode::PrepareImpl(ExecState *) { return Status::OK(); }

Status SourceNode::OpenImpl(ExecState *exec_state) {
  table_ = exec_state->table_store()->GetTable(plan_node_->TableName());
  return Status::OK();
}

Status SourceNode::CloseImpl(ExecState *) { return Status::OK(); }

Status SourceNode::GenerateNextImpl(ExecState *exec_state) {
  DCHECK(table_ != nullptr);

  PL_ASSIGN_OR_RETURN(const auto &row_batch,
                      table_->GetRowBatch(current_chunk_, plan_node_->Columns()));
  PL_RETURN_IF_ERROR(SendRowBatchToChildren(exec_state, *row_batch));
  current_chunk_++;
  return Status::OK();
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
