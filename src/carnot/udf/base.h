#pragma once

#include <memory>
#include <vector>

#include "src/shared/metadata/metadata_state.h"

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

}  // namespace udf
}  // namespace carnot
}  // namespace pl
