#pragma once

#include <arrow/array.h>
#include <arrow/array/builder_base.h>
#include <stddef.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/plan/operators.h"
#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/shared/types/types.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace exec {

constexpr size_t kDefaultUnionRowBatchSize = 1024;

// This node presumes that input streams will always come in ordered by time
// when there is a time column.
class UnionNode : public ProcessingNode {
 public:
  UnionNode() = default;

  // For the time ordered case: A MergeRow represents a row in an input row batch.
  struct MergeRow {
    size_t parent_index;
    size_t row_batch_id;
    size_t row_number;
    types::Time64NSValue time;
    bool end_of_row_batch;
  };

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(
      const plan::Operator& plan_node, const table_store::schema::RowDescriptor& output_descriptor,
      const std::vector<table_store::schema::RowDescriptor>& input_descriptors) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status ConsumeNextImpl(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                         size_t parent_index) override;

 private:
  bool InputsComplete();
  std::shared_ptr<arrow::Array> GetInputColumn(const table_store::schema::RowBatch& rb,
                                               size_t parent_index, size_t column_index);
  Status ConsumeNextUnordered(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                              size_t parent_index);
  Status ConsumeNextOrdered(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                            size_t parent_index);

  size_t num_parents_;
  std::vector<bool> parent_eoses_;

  std::unique_ptr<plan::UnionOperator> plan_node_;
  std::unique_ptr<table_store::schema::RowDescriptor> output_descriptor_;

  // The items below are all for the time-ordered case.

  bool ReadyToMerge();
  void CacheNextRowBatch(size_t parent);
  Status InitializeColumnBuilders();
  types::Time64NSValue GetTimeAtParentCursor(size_t parent_index) const;
  Status AppendRow(size_t parent);
  Status OptionallyFlushRowBatch(ExecState* exec_state);
  Status MergeData(ExecState* exec_state);

  // output_rows_per_batch is only used in the ordered case, because in the unordered case,
  // we just maintain the original row count to avoid copying the data.
  size_t output_rows_per_batch_;

  // Column builders will flush a batch once they hit output_rows_per_batch_ rows.
  std::vector<std::unique_ptr<arrow::ArrayBuilder>> column_builders_;

  // For each parent, mark whether we have received any rows for that particular stream.
  std::vector<bool> received_rows_;
  // Hold onto the input row batches for every parent until we copy all of their data.
  std::vector<std::vector<table_store::schema::RowBatch>> parent_row_batches_;
  // Keep track of where we are in the stream for each parent.
  // The row is always relative to the 'top' row batch that we have for each parent.
  std::vector<size_t> row_cursors_;
  // Cache current working time and data columns for performance reasons.
  std::vector<arrow::Array*> time_columns_;
  std::vector<std::vector<arrow::Array*>> data_columns_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
