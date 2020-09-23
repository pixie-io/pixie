#pragma once

namespace pl {
namespace carnot {
namespace exec {
namespace ml {

enum ModelType {
  kTransformer,
};

/**
 * Base class for all ModelExecutors.
 *
 * Subclasses should implement a static Type method with signature
 *  static constexpr ModelType Type();
 */
class ModelExecutor {};

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace pl
