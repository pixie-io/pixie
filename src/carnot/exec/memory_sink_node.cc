#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "src/carnot/exec/memory_sink_node.h"
#include "src/carnot/exec/table.h"
#include "src/carnot/proto/plan.pb.h"
namespace pl {
namespace carnot {
namespace exec {

std::string MemorySinkNode::DebugStringImpl() {
  return absl::StrFormat("Exec::MemorySinkNode: <name: %s, output: %s>", plan_node_->TableName(),
                         output_descriptor_->DebugString());
}

Status MemorySinkNode::InitImpl(const plan::Operator &plan_node,
                                const RowDescriptor &output_descriptor,
                                const std::vector<RowDescriptor> &) {
  CHECK(plan_node.op_type() == carnotpb::OperatorType::MEMORY_SINK_OPERATOR);
  const auto *sink_plan_node = static_cast<const plan::MemorySinkOperator *>(&plan_node);
  // copy the plan node to local object;
  plan_node_ = std::make_unique<plan::MemorySinkOperator>(*sink_plan_node);
  output_descriptor_ = std::make_unique<RowDescriptor>(output_descriptor);

  return Status::OK();
}
Status MemorySinkNode::PrepareImpl(ExecState *exec_state_) {
  // Create Table.
  table_ = std::make_shared<Table>(*output_descriptor_);
  exec_state_->table_store()->AddTable(plan_node_->TableName(), table_);
  for (size_t i = 0; i < output_descriptor_->size(); i++) {
    auto type = output_descriptor_->type(i);
    PL_RETURN_IF_ERROR(table_->AddColumn(std::make_shared<Column>(type)));
  }

  return Status::OK();
}

Status MemorySinkNode::OpenImpl(ExecState *) { return Status::OK(); }

Status MemorySinkNode::CloseImpl(ExecState *) { return Status::OK(); }

Status MemorySinkNode::ConsumeNextImpl(ExecState *, const RowBatch &rb) {
  DCHECK_EQ(static_cast<size_t>(0), children().size());
  RowBatch output_rb(*output_descriptor_, rb.num_rows());
  PL_RETURN_IF_ERROR(table_->WriteRowBatch(rb));
  return Status::OK();
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
