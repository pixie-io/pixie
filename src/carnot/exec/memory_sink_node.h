#pragma once

#include <stddef.h>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/plan/operators.h"
#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace exec {

class MemorySinkNode : public SinkNode {
 public:
  MemorySinkNode() = default;
  virtual ~MemorySinkNode() = default;

  std::string TableName() const { return plan_node_->TableName(); }

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator& plan_node) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status ConsumeNextImpl(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                         size_t parent_index) override;

 private:
  std::unique_ptr<plan::MemorySinkOperator> plan_node_;
  std::unique_ptr<table_store::schema::RowDescriptor> input_descriptor_;
  std::shared_ptr<table_store::Table> table_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace px
