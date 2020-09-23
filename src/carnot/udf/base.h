#pragma once

#include <memory>
#include <vector>

#include "src/carnot/exec/ml/model_pool.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/types/types.h"

namespace pl {
namespace carnot {
namespace udf {

/**
 * Function context contains contextual resources such as mempools that functions
 * can use while executing.
 */
class FunctionContext {
 public:
  explicit FunctionContext(std::shared_ptr<const md::AgentMetadataState> metadata_state,
                           exec::ml::ModelPool* model_pool)
      : metadata_state_(metadata_state), model_pool_(model_pool) {}
  const pl::md::AgentMetadataState* metadata_state() const { return metadata_state_.get(); }
  exec::ml::ModelPool* model_pool() { return model_pool_; }

 private:
  std::shared_ptr<const pl::md::AgentMetadataState> metadata_state_;
  exec::ml::ModelPool* model_pool_;
};

/**
 * The base for all Carnot funcs.
 */
class BaseFunc {
 public:
  using FunctionContext = ::pl::carnot::udf::FunctionContext;
  using BoolValue = types::BoolValue;
  using Time64NSValue = types::Time64NSValue;
  using Int64Value = types::Int64Value;
  using Float64Value = types::Float64Value;
  using UInt128Value = types::UInt128Value;
  using StringValue = types::StringValue;
};

}  // namespace udf
}  // namespace carnot
}  // namespace pl
