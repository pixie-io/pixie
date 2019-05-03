#include "src/stirling/seq_gen_connector.h"

namespace pl {
namespace stirling {

void SeqGenConnector::TransferDataImpl(types::ColumnWrapperRecordBatch* record_batch) {
  auto& columns = *record_batch;

  std::uniform_int_distribution<uint32_t> num_rows_dist(num_rows_min_, num_rows_max_);

  uint32_t num_records = num_rows_dist(rng_);

  for (uint32_t irecord = 0; irecord < num_records; ++irecord) {
    uint32_t ifield = 0;
    columns[ifield++]->Append<types::Time64NSValue>(time_seq_());
    columns[ifield++]->Append<types::Int64Value>(lin_seq_());
    columns[ifield++]->Append<types::Int64Value>(mod10_seq_());
    columns[ifield++]->Append<types::Int64Value>(square_seq_());
    columns[ifield++]->Append<types::Int64Value>(fib_seq_());
    columns[ifield++]->Append<types::Float64Value>(pi_seq_());
  }
}

}  // namespace stirling
}  // namespace pl
