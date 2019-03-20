#include "src/stirling/seq_gen_connector.h"

namespace pl {
namespace stirling {

void SeqGenConnector::TransferDataImpl(types::ColumnWrapperRecordBatch* record_batch) {
  auto& columns = *record_batch;

  std::uniform_int_distribution<uint32_t> num_rows_dist(num_rows_min_, num_rows_max_);

  uint32_t num_records = num_rows_dist(rng_);

  data_buf_.resize(num_records * elements_.size() * sizeof(uint64_t));

  uint32_t field_size_bytes = 8;
  uint32_t num_fields = elements_.size();

  // If any of the fields is not 8 bytes, this whole scheme breaks
  for (uint32_t ifield = 0; ifield < num_fields; ++ifield) {
    CHECK_EQ(field_size_bytes, elements_[ifield].WidthBytes());
  }

  for (uint32_t irecord = 0; irecord < num_records; ++irecord) {
    for (uint32_t ifield = 0; ifield < num_fields; ++ifield) {
      switch (ifield) {
        case 0:
          columns[ifield]->Append<types::Time64NSValue>(time_seq_());
          break;
        case 1:
          columns[ifield]->Append<types::Int64Value>(lin_seq_());
          break;
        case 2:
          columns[ifield]->Append<types::Int64Value>(mod10_seq_());
          break;
        case 3:
          columns[ifield]->Append<types::Int64Value>(square_seq_());
          break;
        case 4:
          columns[ifield]->Append<types::Int64Value>(fib_seq_());
          break;
        case 5:
          columns[ifield]->Append<types::Float64Value>(pi_seq_());
          break;
      }
    }
  }
}

}  // namespace stirling
}  // namespace pl
