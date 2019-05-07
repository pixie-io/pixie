#include "src/stirling/seq_gen_connector.h"

namespace pl {
namespace stirling {

void SeqGenConnector::TransferDataImpl(uint32_t table_num,
                                       types::ColumnWrapperRecordBatch* record_batch) {
  std::uniform_int_distribution<uint32_t> num_rows_dist(num_rows_min_, num_rows_max_);
  uint32_t num_records = num_rows_dist(rng_);

  auto data_elements = elements(table_num);
  uint32_t num_columns = data_elements.size();

  switch (table_num) {
    case 0:
      TransferDataTable0(num_records, num_columns, record_batch);
      break;
    case 1:
      TransferDataTable1(num_records, num_columns, record_batch);
      break;
    default:
      LOG(ERROR) << absl::StrFormat("Cannot handle the specified table_num %d", table_num);
      ASSERT_TRUE(false);
  }
}

void SeqGenConnector::TransferDataTable0(uint32_t num_records, uint32_t num_columns,
                                         types::ColumnWrapperRecordBatch* record_batch) {
  auto& columns = *record_batch;

  for (uint32_t irecord = 0; irecord < num_records; ++irecord) {
    uint32_t ifield = 0;
    columns[ifield++]->Append<types::Time64NSValue>(table0_time_seq_());
    columns[ifield++]->Append<types::Int64Value>(table0_lin_seq_());
    columns[ifield++]->Append<types::Int64Value>(table0_mod10_seq_());
    columns[ifield++]->Append<types::Int64Value>(table0_square_seq_());
    columns[ifield++]->Append<types::Int64Value>(table0_fib_seq_());
    columns[ifield++]->Append<types::Float64Value>(table0_pi_seq_());
    CHECK_EQ(ifield, num_columns) << absl::StrFormat(
        "Didn't populate all fields [ifield = %d, num_fields = %d]", ifield, num_columns);
  }
}

void SeqGenConnector::TransferDataTable1(uint32_t num_records, uint32_t num_columns,
                                         types::ColumnWrapperRecordBatch* record_batch) {
  auto& columns = *record_batch;
  for (uint32_t irecord = 0; irecord < num_records; ++irecord) {
    uint32_t ifield = 0;
    columns[ifield++]->Append<types::Time64NSValue>(table1_time_seq_());
    columns[ifield++]->Append<types::Int64Value>(table1_lin_seq_());
    columns[ifield++]->Append<types::Int64Value>(table1_mod8_seq_());
    CHECK_EQ(ifield, num_columns) << absl::StrFormat(
        "Didn't populate all fields [ifield = %d, num_fields = %d]", ifield, num_columns);
  }
}

}  // namespace stirling
}  // namespace pl
