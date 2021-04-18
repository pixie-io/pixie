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

#include <stddef.h>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_node.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/expression_evaluator.h"
#include "src/carnot/plan/operators.h"
#include "src/carnot/udf/base.h"
#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {
namespace exec {

class MapNode : public ProcessingNode {
 public:
  MapNode() = default;
  virtual ~MapNode() = default;

 protected:
  std::string DebugStringImpl() override;
  Status InitImpl(const plan::Operator& plan_node) override;
  Status PrepareImpl(ExecState* exec_state) override;
  Status OpenImpl(ExecState* exec_state) override;
  Status CloseImpl(ExecState* exec_state) override;
  Status ConsumeNextImpl(ExecState* exec_state, const table_store::schema::RowBatch& rb,
                         size_t parent_index) override;

 private:
  std::unique_ptr<ExpressionEvaluator> evaluator_;
  std::unique_ptr<plan::MapOperator> plan_node_;
  std::unique_ptr<udf::FunctionContext> function_ctx_;
};

}  // namespace exec
}  // namespace carnot
}  // namespace px
