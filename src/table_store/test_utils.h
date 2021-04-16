#pragma once

#include <memory>
#include <string>
#include <vector>

#include <absl/strings/str_format.h>
#include "src/common/datagen/datagen.h"

namespace px {
namespace table_store {

inline std::vector<std::string> DefaultColumnNames(size_t num_columns) {
  std::vector<std::string> col_names;
  for (size_t col_idx = 0; col_idx < num_columns; col_idx++) {
    col_names.push_back(absl::StrFormat("col%d", col_idx));
  }
  return col_names;
}

inline std::shared_ptr<arrow::Array> GenerateInt64Batch(datagen::DistributionType dist_type,
                                                        int64_t size) {
  if (dist_type == datagen::DistributionType::kUniform) {
    auto data = datagen::CreateLargeData<types::Int64Value>(size);
    return types::ToArrow(data, arrow::default_memory_pool());
  }
  auto data = datagen::GetIntsFromExponential<types::Int64Value>(size, 1);
  return types::ToArrow(data, arrow::default_memory_pool());
}

inline std::shared_ptr<arrow::Array> GenerateStringBatch(
    int size, const datagen::DistributionParams* dist_vars,
    const datagen::DistributionParams* len_vars) {
  auto data = datagen::CreateLargeStringData(size, dist_vars, len_vars);
  return types::ToArrow<types::StringValue>(data.ConsumeValueOrDie(), arrow::default_memory_pool());
}

inline StatusOr<std::shared_ptr<Table>> CreateTable(
    std::vector<std::string> col_names, std::vector<types::DataType> types,
    std::vector<datagen::DistributionType> distribution_types, int64_t rb_size, int64_t num_batches,
    const datagen::DistributionParams* dist_vars, const datagen::DistributionParams* len_vars) {
  schema::RowDescriptor rd(types);

  auto table = Table::Create(table_store::schema::Relation(types, col_names));

  for (size_t col_idx = 0; col_idx < types.size(); col_idx++) {
    auto col = table->GetColumn(col_idx);
    for (int batch_idx = 0; batch_idx < num_batches; batch_idx++) {
      std::shared_ptr<arrow::Array> batch;
      if (types.at(col_idx) == types::DataType::INT64) {
        batch = GenerateInt64Batch(distribution_types.at(col_idx), rb_size);
      } else if (types.at(col_idx) == types::DataType::STRING) {
        batch = GenerateStringBatch(rb_size, dist_vars, len_vars);
      } else {
        return error::InvalidArgument("We only support int and str types.");
      }
      auto s = col->AddBatch(batch);
      PL_UNUSED(s);
    }
  }

  return table;
}

inline StatusOr<std::shared_ptr<Table>> CreateTable(
    std::vector<types::DataType> types, std::vector<datagen::DistributionType> distribution_types,
    int64_t rb_size, int64_t num_batches, const datagen::DistributionParams* dist_vars,
    const datagen::DistributionParams* len_vars) {
  return CreateTable(DefaultColumnNames(types.size()), types, distribution_types, rb_size,
                     num_batches, dist_vars, len_vars);
}

}  // namespace table_store
}  // namespace px
