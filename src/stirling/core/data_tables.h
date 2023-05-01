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
#include <utility>
#include <vector>

#include "src/stirling/core/data_table.h"
#include "src/stirling/core/types.h"

namespace px {
namespace stirling {

class DataTables {
 public:
  template <size_t NumTableSchemas>
  explicit DataTables(
      const std::array<px::stirling::DataTableSchema, NumTableSchemas>& table_schemas) {
    for (const auto& [id, table_schema] : Enumerate(table_schemas)) {
      auto data_table = std::make_unique<DataTable>(id, table_schema);
      data_table_ptrs_.push_back(data_table.get());
      data_table_uptrs_.push_back(std::move(data_table));
    }
  }

  std::vector<DataTable*> tables() { return data_table_ptrs_; }

  DataTable* operator[](int idx) { return data_table_ptrs_[idx]; }

 private:
  std::vector<std::unique_ptr<DataTable>> data_table_uptrs_;
  std::vector<DataTable*> data_table_ptrs_;
};

}  // namespace stirling
}  // namespace px
