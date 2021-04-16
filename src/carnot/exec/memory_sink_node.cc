#include "src/carnot/exec/memory_sink_node.h"

#include <string>
#include <vector>

#include <absl/strings/substitute.h>

#include "src/carnot/planpb/plan.pb.h"
#include "src/common/base/base.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::Column;
using table_store::Table;
using table_store::schema::Relation;
using table_store::schema::RowBatch;
using table_store::schema::RowDescriptor;

std::string MemorySinkNode::DebugStringImpl() {
  return absl::Substitute("Exec::MemorySinkNode: {name: $0, output: $1}", plan_node_->TableName(),
                          input_descriptor_->DebugString());
}

Status MemorySinkNode::InitImpl(const plan::Operator& plan_node) {
  CHECK(plan_node.op_type() == planpb::OperatorType::MEMORY_SINK_OPERATOR);
  const auto* sink_plan_node = static_cast<const plan::MemorySinkOperator*>(&plan_node);
  // copy the plan node to local object;
  plan_node_ = std::make_unique<plan::MemorySinkOperator>(*sink_plan_node);
  if (input_descriptors_.size() != 1) {
    return error::InvalidArgument("MemorySink operator expects a single input relation, got $0",
                                  input_descriptors_.size());
  }
  input_descriptor_ = std::make_unique<table_store::schema::RowDescriptor>(input_descriptors_[0]);

  return Status::OK();
}

Status MemorySinkNode::PrepareImpl(ExecState* exec_state_) {
  // Create Table.
  std::vector<std::string> col_names;
  for (size_t i = 0; i < input_descriptor_->size(); i++) {
    col_names.push_back(plan_node_->ColumnName(i));
  }

  table_ = Table::Create(Relation(input_descriptor_->types(), col_names));
  exec_state_->table_store()->AddTable(plan_node_->TableName(), table_);

  return Status::OK();
}

Status MemorySinkNode::OpenImpl(ExecState*) { return Status::OK(); }

Status MemorySinkNode::CloseImpl(ExecState*) { return Status::OK(); }

Status MemorySinkNode::ConsumeNextImpl(ExecState*, const RowBatch& rb, size_t) {
  DCHECK_EQ(static_cast<size_t>(0), children().size());
  if (rb.num_rows() > 0 || (rb.eow() || rb.eos())) {
    PL_RETURN_IF_ERROR(table_->WriteRowBatch(rb));
  }
  return Status::OK();
}

}  // namespace exec
}  // namespace carnot
}  // namespace px
