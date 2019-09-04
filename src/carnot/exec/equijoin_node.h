#pragma once

#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/row_tuple.h"
#include "src/common/memory/memory.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace carnot {
namespace exec {

constexpr size_t kDefaultJoinRowBatchSize = 1024;

class EquijoinNode : public ProcessingNode {
  enum class JoinInputTable { kLeftTable, kRightTable };

  struct TableSpec {
    bool emit_unmatched_rows;
    // Indices of the input columns for the keys.
    std::vector<int64_t> key_indices;
    // Indices of the input columns that are outputted by the build/probe.
    std::vector<int64_t> input_col_indices;
    std::vector<types::DataType> input_col_types;
    // For each of the values in input_col_indices, the index in the output row batch
    // that column should be written to.
    std::vector<int64_t> output_col_indices;
  };

 public:
  EquijoinNode() = default;

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
  Status InitializeColumnBuilders();
  bool IsProbeTable(size_t parent_index);

  // Note whether the left or the right table is the probe table.
  JoinInputTable probe_table_;
  // output_rows_per_batch is only used in the ordered case, because in the unordered case,
  // we just maintain the original row count to avoid copying the data.
  int64_t output_rows_per_batch_;

  // Specification for the join for each of the input tables.
  TableSpec build_spec_;
  TableSpec probe_spec_;

  std::vector<types::DataType> key_data_types_;

  // Column builders will flush a batch once they hit output_rows_per_batch_ rows.
  std::vector<std::unique_ptr<arrow::ArrayBuilder>> column_builders_;

  std::unique_ptr<plan::JoinOperator> plan_node_;
  std::unique_ptr<table_store::schema::RowDescriptor> output_descriptor_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace pl
