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

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/plan/operators.h"
#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/table_store/schema/row_batch.h"
#include "src/table_store/table/table.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace exec {

using table_store::Table;
using table_store::schema::RowBatch;

class MemorySourceNode : public SourceNode {
 public:
  MemorySourceNode() = default;
  virtual ~MemorySourceNode() = default;

  bool NextBatchReady() override;

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator& plan_node) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status GenerateNextImpl(ExecState* exec_state) override;

 private:
  StatusOr<std::unique_ptr<RowBatch>> GetNextRowBatch(ExecState* exec_state);
  bool InfiniteStreamNextBatchReady();
  // Whether this memory source will stream future results.
  bool streaming_ = false;

  std::unique_ptr<Table::Cursor> cursor_;

  std::unique_ptr<plan::MemorySourceOperator> plan_node_;
  table_store::Table* table_ = nullptr;
};

}  // namespace exec
}  // namespace carnot
}  // namespace px
