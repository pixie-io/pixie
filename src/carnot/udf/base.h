#pragma once

#include <memory>
#include <vector>

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
  explicit FunctionContext(std::shared_ptr<const md::AgentMetadataState> metadata_state)
      : metadata_state_(metadata_state) {}
  const pl::md::AgentMetadataState* metadata_state() const { return metadata_state_.get(); }

 private:
  std::shared_ptr<const pl::md::AgentMetadataState> metadata_state_;
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
