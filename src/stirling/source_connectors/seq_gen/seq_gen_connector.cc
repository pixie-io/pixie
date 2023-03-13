/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/stirling/source_connectors/seq_gen/seq_gen_connector.h"

namespace px {
namespace stirling {

Status SeqGenConnector::InitImpl() {
  sampling_freq_mgr_.set_period(kSamplingPeriod);
  push_freq_mgr_.set_period(kPushPeriod);
  return Status::OK();
}

void SeqGenConnector::TransferDataImpl(ConnectorContext* /* ctx */) {
  DCHECK_EQ(data_tables_.size(), 2U);

  std::uniform_int_distribution<uint32_t> num_rows_dist(num_rows_min_, num_rows_max_);

  if (data_tables_[kSeq0TableNum] != nullptr) {
    uint32_t num_records = num_rows_dist(rng_);
    TransferDataTable0(num_records, data_tables_[kSeq0TableNum]);
  }

  if (data_tables_[kSeq1TableNum] != nullptr) {
    uint32_t num_records = num_rows_dist(rng_);
    TransferDataTable1(num_records, data_tables_[kSeq1TableNum]);
  }
}

void SeqGenConnector::TransferDataTable0(uint32_t num_records, DataTable* data_table) {
  for (uint32_t irecord = 0; irecord < num_records; ++irecord) {
    DataTable::RecordBuilder<&kSeq0Table> r(data_table);
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
    auto table1_mode8_seq_val = table1_mod8_seq_();

    types::TabletID tablet_id = std::to_string(table1_mode8_seq_val);

    DataTable::RecordBuilder<&kSeq1Table> r(data_table, tablet_id);
    r.Append<r.ColIndex("time_")>(table1_time_seq_());
    r.Append<r.ColIndex("x")>(table1_lin_seq_());
    // Tabletization key must also be appended as a column value.
    // See note in RecordBuilder class.
    r.Append<r.ColIndex("xmod8")>(table1_mode8_seq_val);
  }
}

}  // namespace stirling
}  // namespace px
