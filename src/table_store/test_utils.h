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

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <absl/strings/str_format.h>
#include "schema/row_batch.h"
#include "schema/row_descriptor.h"
#include "src/datagen/datagen.h"

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

  auto table = Table::Create("test_table", table_store::schema::Relation(types, col_names));

  for (int batch_idx = 0; batch_idx < num_batches; batch_idx++) {
    auto rb = schema::RowBatch(schema::RowDescriptor(types), rb_size);
    for (size_t col_idx = 0; col_idx < types.size(); col_idx++) {
      std::shared_ptr<arrow::Array> batch;
      if (types.at(col_idx) == types::DataType::INT64) {
        batch = GenerateInt64Batch(distribution_types.at(col_idx), rb_size);
      } else if (types.at(col_idx) == types::DataType::STRING) {
        batch = GenerateStringBatch(rb_size, dist_vars, len_vars);
      } else {
        return error::InvalidArgument("We only support int and str types.");
      }
      PX_CHECK_OK(rb.AddColumn(batch));
    }
    PX_CHECK_OK(table->WriteRowBatch(rb));
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
