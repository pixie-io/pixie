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
#include <vector>

#include "src/carnot/udf/model_pool.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace udf {

/**
 * Function context contains contextual resources such as mempools that functions
 * can use while executing.
 */
class FunctionContext {
 public:
  explicit FunctionContext(std::shared_ptr<const md::AgentMetadataState> metadata_state,
                           ModelPool* model_pool)
      : metadata_state_(metadata_state), model_pool_(model_pool) {}
  const px::md::AgentMetadataState* metadata_state() const { return metadata_state_.get(); }
  ModelPool* model_pool() { return model_pool_; }

 private:
  std::shared_ptr<const px::md::AgentMetadataState> metadata_state_;
  ModelPool* model_pool_;
};

/**
 * The base for all Carnot funcs.
 */
class BaseFunc {
 public:
  using FunctionContext = ::px::carnot::udf::FunctionContext;
  using BoolValue = types::BoolValue;
  using Time64NSValue = types::Time64NSValue;
  using Int64Value = types::Int64Value;
  using Float64Value = types::Float64Value;
  using UInt128Value = types::UInt128Value;
  using StringValue = types::StringValue;
};

}  // namespace udf
}  // namespace carnot
}  // namespace px
