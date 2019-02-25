#include "src/stirling/seq_gen_connector.h"

namespace pl {
namespace stirling {

RawDataBuf SeqGenConnector::GetDataImpl() {
  std::uniform_int_distribution<uint32_t> num_rows_dist(num_rows_min_, num_rows_max_);

  uint32_t num_records = num_rows_dist(rng_);

  data_buf_.resize(num_records * elements_.size() * sizeof(uint64_t));

  auto int64_data_buf_ptr = reinterpret_cast<int64_t *>(data_buf_.data());
  auto double_data_buf_ptr = reinterpret_cast<double *>(data_buf_.data());

  uint32_t field_size_bytes = 8;
  uint32_t num_fields = elements_.size();

  // If any of the fields is not 8 bytes, this whole scheme breaks
  for (uint32_t ifield = 0; ifield < num_fields; ++ifield) {
    CHECK_EQ(field_size_bytes, elements_[ifield].WidthBytes());
  }

  for (uint32_t irecord = 0; irecord < num_records; ++irecord) {
    for (uint32_t ifield = 0; ifield < num_fields; ++ifield) {
      uint32_t buf_idx = irecord * num_fields + ifield;

      switch (ifield) {
        case 0:
          int64_data_buf_ptr[buf_idx] = time_seq_();
          break;
        case 1:
          int64_data_buf_ptr[buf_idx] = lin_seq_();
          break;
        case 2:
          int64_data_buf_ptr[buf_idx] = mod10_seq_();
          break;
        case 3:
          int64_data_buf_ptr[buf_idx] = square_seq_();
          break;
        case 4:
          int64_data_buf_ptr[buf_idx] = fib_seq_();
          break;
        case 5:
          double_data_buf_ptr[buf_idx] = pi_seq_();
          break;
      }
    }
  }

  return RawDataBuf(num_records, data_buf_.data());
}

}  // namespace stirling
}  // namespace pl
