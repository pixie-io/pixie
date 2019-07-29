#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/shared/types/type_utils.h"
#include "src/stirling/data_table.h"
#include "src/stirling/types.h"

namespace pl {
namespace stirling {

using types::ColumnWrapper;
using types::DataType;

DataTable::DataTable(const InfoClassSchema& schema) {
  DCHECK_EQ(table_schema_.size(), 0ULL);
  for (const auto& info_class_element : schema) {
    table_schema_.emplace_back(info_class_element);
  }
  sealed_batches_ =
      std::make_unique<std::vector<std::unique_ptr<types::ColumnWrapperRecordBatch>>>();
  InitBuffers();
}

DataTable::DataTable(const DataTableSchema& schema) : DataTable(InfoClassSchema(schema)) {}

void DataTable::InitBuffers() {
  DCHECK(record_batch_ == nullptr);

  record_batch_ = std::make_unique<types::ColumnWrapperRecordBatch>();

  for (const auto& element : table_schema_) {
    pl::types::DataType type = element.type();

#define TYPE_CASE(_dt_)                           \
  auto col = types::ColumnWrapper::Make(_dt_, 0); \
  col->Reserve(kTargetCapacity);                  \
  record_batch_->push_back(col);
    PL_SWITCH_FOREACH_DATATYPE(type, TYPE_CASE);
#undef TYPE_CASE
  }
}

std::unique_ptr<types::ColumnWrapperRecordBatchVec> DataTable::ConsumeRecordBatches() {
  SealActiveRecordBatch();

  auto sealed_batches_uptr = std::move(sealed_batches_);

  sealed_batches_ = std::make_unique<types::ColumnWrapperRecordBatchVec>();

  return sealed_batches_uptr;
}

void DataTable::SealActiveRecordBatch() {
  for (uint32_t i = 0; i < table_schema_.size(); ++i) {
    auto col = (*record_batch_)[i];
    col->ShrinkToFit();
  }
  sealed_batches_->push_back(std::move(record_batch_));
  InitBuffers();
}

}  // namespace stirling
}  // namespace pl
