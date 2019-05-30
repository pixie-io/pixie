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
  table_schema_ = std::make_unique<std::vector<DataElement>>();
  for (const auto& info_class_element : schema) {
    table_schema_->emplace_back(info_class_element);
  }
  sealed_batches_ =
      std::make_unique<std::vector<std::unique_ptr<types::ColumnWrapperRecordBatch>>>();
  PL_CHECK_OK(InitBuffers());
}

Status DataTable::InitBuffers() {
  DCHECK(record_batch_ == nullptr);

  record_batch_ = std::make_unique<types::ColumnWrapperRecordBatch>();

  return InitRecordBatch(*table_schema_, target_capacity_, record_batch_.get());
}

StatusOr<std::unique_ptr<types::ColumnWrapperRecordBatchVec>> DataTable::GetRecordBatches() {
  Status s = SealActiveRecordBatch();
  PL_RETURN_IF_ERROR(s);

  auto sealed_batches_ptr = std::move(sealed_batches_);

  sealed_batches_ = std::make_unique<types::ColumnWrapperRecordBatchVec>();

  return std::move(sealed_batches_ptr);
}

Status DataTable::SealActiveRecordBatch() {
  for (uint32_t i = 0; i < table_schema_->size(); ++i) {
    auto col = (*record_batch_)[i];
    col->ShrinkToFit();
  }
  sealed_batches_->push_back(std::move(record_batch_));
  Status s = InitBuffers();
  PL_RETURN_IF_ERROR(s);

  return Status::OK();
}

}  // namespace stirling
}  // namespace pl
