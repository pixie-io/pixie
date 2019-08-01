#include "src/stirling/seq_gen_connector.h"

namespace pl {
namespace stirling {

void SeqGenConnector::TransferDataImpl(ConnectorContext* /* ctx */, uint32_t table_num,
                                       DataTable* data_table) {
  std::uniform_int_distribution<uint32_t> num_rows_dist(num_rows_min_, num_rows_max_);
  uint32_t num_records = num_rows_dist(rng_);

  switch (table_num) {
    case kSeq0TableNum:
      TransferDataTable0(num_records, data_table);
      break;
    case kSeq1TableNum:
      TransferDataTable1(num_records, data_table);
      break;
    default:
      LOG(ERROR) << absl::Substitute("Cannot handle the specified table_num $0", table_num);
      ASSERT_TRUE(false);
  }
}

void SeqGenConnector::TransferDataTable0(uint32_t num_records, DataTable* data_table) {
  for (uint32_t irecord = 0; irecord < num_records; ++irecord) {
    RecordBuilder<&kSeq0Table> r(data_table);
    r.Append<r.ColIndex("time_")>(table0_time_seq_());
    r.Append<r.ColIndex("x")>(table0_lin_seq_());
    r.Append<r.ColIndex("xmod10")>(table0_mod10_seq_());
    r.Append<r.ColIndex("xsquared")>(table0_square_seq_());
    r.Append<r.ColIndex("fibonnaci")>(table0_fib_seq_());
    r.Append<r.ColIndex("PIx")>(table0_pi_seq_());
  }
}

void SeqGenConnector::TransferDataTable1(uint32_t num_records, DataTable* data_table) {
  for (uint32_t irecord = 0; irecord < num_records; ++irecord) {
    auto tablet_id = table1_mod8_seq_();
    RecordBuilder<&kSeq1Table> r(data_table, tablet_id);
    r.Append<r.ColIndex("time_")>(table1_time_seq_());
    r.Append<r.ColIndex("x")>(table1_lin_seq_());
    // Tabletization key must also be appended as a column value.
    // See note in RecordBuilder class.
    r.Append<r.ColIndex("xmod8")>(tablet_id);
  }
}

}  // namespace stirling
}  // namespace pl
