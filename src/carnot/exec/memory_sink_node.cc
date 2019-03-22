#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "src/carnot/exec/memory_sink_node.h"
#include "src/carnot/proto/plan.pb.h"
#include "src/carnot/schema/relation.h"
#include "src/carnot/schema/table.h"

namespace pl {
namespace carnot {
namespace exec {

using schema::Column;
using schema::Relation;
using schema::RowBatch;
using schema::RowDescriptor;
using schema::Table;

std::string MemorySinkNode::DebugStringImpl() {
  return absl::StrFormat("Exec::MemorySinkNode: {name: %s, output: %s}", plan_node_->TableName(),
                         input_descriptor_->DebugString());
}

Status MemorySinkNode::InitImpl(const plan::Operator &plan_node, const schema::RowDescriptor &,
                                const std::vector<schema::RowDescriptor> &input_descriptors) {
  CHECK(plan_node.op_type() == carnotpb::OperatorType::MEMORY_SINK_OPERATOR);
  const auto *sink_plan_node = static_cast<const plan::MemorySinkOperator *>(&plan_node);
  // copy the plan node to local object;
  plan_node_ = std::make_unique<plan::MemorySinkOperator>(*sink_plan_node);
  input_descriptor_ = std::make_unique<schema::RowDescriptor>(input_descriptors[0]);

  return Status::OK();
}
Status MemorySinkNode::PrepareImpl(ExecState *exec_state_) {
  // Create Table.
  std::vector<std::string> col_names;
  for (size_t i = 0; i < input_descriptor_->size(); i++) {
    col_names.push_back(plan_node_->ColumnName(i));
  }

  table_ = std::make_shared<Table>(Relation(input_descriptor_->types(), col_names));
  exec_state_->table_store()->AddTable(plan_node_->TableName(), table_);

  return Status::OK();
}

Status MemorySinkNode::OpenImpl(ExecState *) { return Status::OK(); }

Status MemorySinkNode::CloseImpl(ExecState *) { return Status::OK(); }

Status MemorySinkNode::ConsumeNextImpl(ExecState *, const RowBatch &rb) {
  DCHECK_EQ(static_cast<size_t>(0), children().size());
  PL_RETURN_IF_ERROR(table_->WriteRowBatch(rb));
  return Status::OK();
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
