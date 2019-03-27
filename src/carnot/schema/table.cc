#include <algorithm>
#include <cmath>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/carnot/schema/relation.h"
#include "src/carnot/schema/table.h"
#include "src/common/base/base.h"
#include "src/shared/types/arrow_adapter.h"
#include "src/shared/types/type_utils.h"

namespace pl {
namespace carnot {
namespace schema {

using types::DataType;

template <DataType T>
auto GetPBDataColumn(schemapb::Column* /*data_col*/) {
  static_assert(sizeof(T) != 0, "Unsupported data type");
}

template <>
auto GetPBDataColumn<DataType::BOOLEAN>(schemapb::Column* data_col) {
  return data_col->mutable_boolean_data();
}

template <>
auto GetPBDataColumn<DataType::TIME64NS>(schemapb::Column* data_col) {
  return data_col->mutable_time64ns_data();
}

template <>
auto GetPBDataColumn<DataType::INT64>(schemapb::Column* data_col) {
  return data_col->mutable_int64_data();
}

template <>
auto GetPBDataColumn<DataType::FLOAT64>(schemapb::Column* data_col) {
  return data_col->mutable_float64_data();
}

template <>
auto GetPBDataColumn<DataType::STRING>(schemapb::Column* data_col) {
  return data_col->mutable_string_data();
}

template <DataType T>
void CopyIntoOutputPB(schemapb::Column* data_col, arrow::Array* col) {
  CHECK(data_col != nullptr);
  CHECK(col != nullptr);

  size_t col_length = col->length();
  auto casted_output_data = GetPBDataColumn<T>(data_col);
  for (size_t i = 0; i < col_length; ++i) {
    casted_output_data->add_data(types::GetValueFromArrowArray<T>(col, i));
  }
}

/**
 * Converts an arrow batch to a proto row batch.
 * @param rb The rowbatch.
 * @param relation The relation.
 * @param rb_data_pb The output proto.
 * @return Status of conversion.
 */
Status ConvertRecordBatchToProto(arrow::RecordBatch* rb,
                                 const pl::carnot::schema::Relation& relation,
                                 schemapb::RowBatchData* rb_data_pb) {
  for (int col_idx = 0; col_idx < rb->num_columns(); ++col_idx) {
    auto col = rb->column(col_idx);
    auto output_col_data = rb_data_pb->add_cols();
    auto dt = relation.GetColumnType(col_idx);

#define TYPE_CASE(_dt_) CopyIntoOutputPB<_dt_>(output_col_data, col.get());

    PL_SWITCH_FOREACH_DATATYPE(dt, TYPE_CASE);
#undef TYPE_CASE
  }
  return Status::OK();
}

Table::Table(const schema::Relation& relation) : desc_(relation.col_types()) {
  uint64_t num_cols = desc_.size();
  columns_.reserve(num_cols);
  for (uint64_t i = 0; i < num_cols; ++i) {
    PL_CHECK_OK(
        AddColumn(std::make_shared<Column>(relation.GetColumnType(i), relation.GetColumnName(i))));
  }
}

Status Column::AddBatch(const std::shared_ptr<arrow::Array>& batch) {
  // Check type and check size.
  if (types::ToArrowType(data_type_) != batch->type_id()) {
    return error::InvalidArgument("Column is of type $0, but needs to be type $1.",
                                  batch->type_id(), data_type_);
  }

  batches_.emplace_back(batch);
  return Status::OK();
}

Status Table::AddColumn(std::shared_ptr<Column> col) {
  // Check number of columns.
  if (columns_.size() >= desc_.size()) {
    return error::InvalidArgument("Table has too many columns.");
  }

  // Check type of column.
  if (col->data_type() != desc_.type(columns_.size())) {
    return error::InvalidArgument("Column was is of type $0, but needs to be type $1.",
                                  col->data_type(), desc_.type(columns_.size()));
  }

  if (!columns_.empty()) {
    // Check number of batches.
    if (columns_[0]->numBatches() != col->numBatches()) {
      return error::InvalidArgument("Column has $0 batches, but should have $1.", col->numBatches(),
                                    columns_[0]->numBatches());
    }
    // Check size of batches.
    for (int64_t batch_idx = 0; batch_idx < columns_[0]->numBatches(); batch_idx++) {
      if (columns_[0]->batch(batch_idx)->length() != col->batch(batch_idx)->length()) {
        return error::InvalidArgument("Column has batch of size $0, but should have size $1.",
                                      col->batch(batch_idx)->length(),
                                      columns_[0]->batch(batch_idx)->length());
      }
    }
  }

  columns_.emplace_back(col);
  name_to_column_map_.emplace(col->name(), col);
  return Status::OK();
}

Status Table::ToProto(schemapb::Table* table_proto) const {
  CHECK(table_proto != nullptr);
  auto row_batch_proto = table_proto->add_row_batches();
  auto relation = GetRelation();
  // TODO(zasgar): Table should store record batches, that way we can reduce conversions.
  PL_ASSIGN_OR_RETURN(auto batches, GetTableAsRecordBatches());
  for (const auto batch : batches) {
    PL_RETURN_IF_ERROR(ConvertRecordBatchToProto(batch.get(), relation, row_batch_proto));
  }
  PL_RETURN_IF_ERROR(relation.ToProto(table_proto->mutable_relation()));
  return Status::OK();
}

StatusOr<std::unique_ptr<RowBatch>> Table::GetRowBatch(int64_t row_batch_idx,
                                                       std::vector<int64_t> cols,
                                                       arrow::MemoryPool* mem_pool) const {
  return GetRowBatchSlice(row_batch_idx, cols, mem_pool, 0 /* offset */, -1 /* end */);
}

StatusOr<std::unique_ptr<RowBatch>> Table::GetRowBatchSlice(int64_t row_batch_idx,
                                                            std::vector<int64_t> cols,
                                                            arrow::MemoryPool* mem_pool,
                                                            int64_t offset, int64_t end) const {
  DCHECK(!columns_.empty()) << "RowBatch does not have any columns.";
  DCHECK(NumBatches() > row_batch_idx) << absl::StrFormat(
      "Table has %d batches, but requesting batch %d", NumBatches(), row_batch_idx);

  // Get column types for row descriptor.
  std::vector<types::DataType> rb_types;
  for (auto col_idx : cols) {
    rb_types.push_back(desc_.type(col_idx));
  }

  auto num_cold_batches = !columns_.empty() ? columns_[0]->numBatches() : 0;
  // If i > num_cold_batches, hot_idx is the index of the batch that we want from the hot columns.
  auto hot_idx = row_batch_idx - num_cold_batches;

  {
    absl::base_internal::SpinLockHolder lock(&hot_batches_lock_);

    if (hot_idx >= 0) {
      DCHECK(hot_batches_.size() > static_cast<size_t>(hot_idx));
      // Move hot column batches 0 to hot_idx into cold storage.
      // TODO(michelle): (PL-388) We're currently converting hot data to row batches on a 1:1 basis.
      // This should be updated so that multiple hot column batches are merged into a single row
      // batch.
      auto batch_idx = 0;
      while (batch_idx <= hot_idx) {
        for (size_t col_idx = 0; col_idx < columns_.size(); col_idx++) {
          DCHECK(hot_batches_[batch_idx]->size() > col_idx);
          auto hot_batch_sptr = hot_batches_[batch_idx]->at(col_idx)->ConvertToArrow(mem_pool);
          PL_RETURN_IF_ERROR(columns_.at(col_idx)->AddBatch(hot_batch_sptr));
        }
        batch_idx++;
      }
      // Remove hot column batches 0 to hot_idx from hot columns.
      hot_batches_.erase(hot_batches_.begin(), hot_batches_.begin() + hot_idx + 1);
    }
  }

  DCHECK_GT(columns_.size(), static_cast<size_t>(0));
  DCHECK(columns_[0]->numBatches() > row_batch_idx);
  auto batch_size =
      (end == -1) ? (columns_[0]->batch(row_batch_idx)->length() - offset) : (end - offset);
  auto output_rb = std::make_unique<RowBatch>(RowDescriptor(rb_types), batch_size);
  for (auto col_idx : cols) {
    auto arrow_array_sptr = columns_[col_idx]->batch(row_batch_idx);
    PL_RETURN_IF_ERROR(output_rb->AddColumn(arrow_array_sptr->Slice(offset, batch_size)));
  }

  return output_rb;
}

Status Table::WriteRowBatch(RowBatch rb) {
  if (rb.desc().size() != desc_.size()) {
    return error::InvalidArgument(
        "RowBatch's row descriptor length ($0) does not match table's row descriptor length ($1).",
        rb.desc().size(), desc_.size());
  }
  for (size_t i = 0; i < rb.desc().size(); i++) {
    if (rb.desc().type(i) != desc_.type(i)) {
      return error::InvalidArgument(
          "RowBatch's row descriptor does not match table's row descriptor.");
    }
  }

  for (int64_t i = 0; i < rb.num_columns(); i++) {
    auto s = columns_[i]->AddBatch(rb.ColumnAt(i));
    PL_RETURN_IF_ERROR(s);
  }
  return Status::OK();
}

Status Table::TransferRecordBatch(
    std::unique_ptr<pl::types::ColumnWrapperRecordBatch> record_batch) {
  // Check for matching types
  auto received_num_columns = record_batch->size();
  auto expected_num_columns = desc_.size();
  CHECK_EQ(expected_num_columns, received_num_columns)
      << absl::StrFormat("Table schema mismatch: expected=%u received=%u)", expected_num_columns,
                         received_num_columns);

  uint32_t i = 0;
  for (const auto& col : *record_batch) {
    auto received_type = col->data_type();
    auto expected_type = desc_.type(i);
    DCHECK_EQ(expected_type, received_type)
        << absl::StrFormat("Type mismatch [column=%u]: expected=%s received=%s", i,
                           ToString(expected_type), ToString(received_type));
    ++i;
  }

  absl::base_internal::SpinLockHolder lock(&hot_batches_lock_);
  hot_batches_.push_back(std::move(record_batch));

  return Status::OK();
}

int64_t Table::NumBatches() const {
  auto num_batches = 0;
  if (!columns_.empty()) {
    num_batches += columns_[0]->numBatches();
  }

  absl::base_internal::SpinLockHolder lock(&hot_batches_lock_);
  num_batches += hot_batches_.size();

  return num_batches;
}

int64_t Table::FindTimeColumn() {
  int64_t time_col_idx = -1;
  for (size_t i = 0; i < columns_.size(); i++) {
    if (columns_[i]->name() == "time_") {
      time_col_idx = i;
      break;
    }
  }
  return time_col_idx;
}

std::shared_ptr<arrow::Array> Table::GetColumnBatch(int64_t col, int64_t batch,
                                                    arrow::MemoryPool* mem_pool) {
  DCHECK(static_cast<int64_t>(NumBatches()) > batch);
  DCHECK(static_cast<int64_t>(columns_.size()) > col);

  if (batch >= columns_[col]->numBatches()) {
    auto hot_col_idx = batch - columns_[col]->numBatches();
    return hot_batches_.at(hot_col_idx)->at(col)->ConvertToArrow(mem_pool);
  }
  return columns_[col]->batch(batch);
}

int64_t Table::FindBatchGreaterThanOrEqual(int64_t time_col_idx, int64_t time,
                                           arrow::MemoryPool* mem_pool) {
  DCHECK(columns_[time_col_idx]->data_type() == types::DataType::TIME64NS);
  return FindBatchGreaterThanOrEqual(time_col_idx, time, mem_pool, 0, NumBatches() - 1);
}

BatchPosition Table::FindBatchPositionGreaterThanOrEqual(int64_t time,
                                                         arrow::MemoryPool* mem_pool) {
  BatchPosition batch_pos = {-1, -1};

  int64_t time_col_idx = FindTimeColumn();
  DCHECK_NE(time_col_idx, -1);

  batch_pos.batch_idx = FindBatchGreaterThanOrEqual(time_col_idx, time, mem_pool);

  if (batch_pos.FoundValidBatches()) {
    // If batch in range exists, find the specific row in the batch.
    std::shared_ptr<arrow::Array> batch =
        GetColumnBatch(time_col_idx, batch_pos.batch_idx, mem_pool);
    batch_pos.row_idx =
        types::SearchArrowArrayGreaterThanOrEqual<types::DataType::INT64>(batch.get(), time);
  }
  return batch_pos;
}

int64_t Table::FindBatchGreaterThanOrEqual(int64_t time_col_idx, int64_t time,
                                           arrow::MemoryPool* mem_pool, int64_t start,
                                           int64_t end) {
  if (start > end) {
    return -1;
  }

  int64_t mid = (start + end) / 2;
  auto batch = GetColumnBatch(time_col_idx, mid, mem_pool);
  auto start_val = types::GetValueFromArrowArray<types::DataType::INT64>(batch.get(), 0);
  auto stop_val =
      types::GetValueFromArrowArray<types::DataType::INT64>(batch.get(), batch->length() - 1);
  if (time > start_val && time <= stop_val) {
    return mid;
  }
  if (time > stop_val) {
    return FindBatchGreaterThanOrEqual(time_col_idx, time, mem_pool, mid + 1, end);
  }
  if (time <= start_val) {
    auto res = FindBatchGreaterThanOrEqual(time_col_idx, time, mem_pool, start, mid - 1);
    if (res == -1) {
      return mid;
    }
    return res;
  }
  return -1;
}

schema::Relation Table::GetRelation() const {
  std::vector<types::DataType> types;
  std::vector<std::string> names;
  types.reserve(columns_.size());
  names.reserve(columns_.size());

  for (const auto& col : columns_) {
    types.push_back(col->data_type());
    names.push_back(col->name());
  }

  return schema::Relation(types, names);
}

StatusOr<std::vector<RecordBatchSPtr>> Table::GetTableAsRecordBatches() const {
  std::vector<int64_t> col_selector;
  // Set up the schema.
  std::vector<std::shared_ptr<arrow::Field>> schema_vector = {};
  for (int64_t i = 0; i < NumColumns(); i++) {
    auto col = GetColumn(i);
    col_selector.push_back(i);
    schema_vector.push_back(arrow::field(col->name(), col->batch(0)->type()));
  }
  auto schema = std::make_shared<arrow::Schema>(schema_vector);

  // Setup the records.
  std::vector<RecordBatchSPtr> record_batches;
  for (int64_t i = 0; i < NumBatches(); i++) {
    // Get the row batch.
    PL_ASSIGN_OR_RETURN(auto cur_rb, GetRowBatch(i, col_selector, arrow::default_memory_pool()));
    // Break apart the row batch to get the columns.
    auto record_batch = arrow::RecordBatch::Make(schema, cur_rb->num_rows(), cur_rb->columns());
    record_batches.push_back(record_batch);
  }
  return record_batches;
}
}  // namespace schema
}  // namespace carnot
}  // namespace pl
