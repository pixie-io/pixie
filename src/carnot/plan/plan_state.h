#pragma once

#include <memory>

namespace px {
namespace carnot {
namespace udf {
// Forward declare registries.
class Registry;
}  // namespace udf
namespace plan {

class PlanState {
 public:
  /**
   * Init with a UDF registry and hold a raw pointer for it.
   *
   * @param func_registry the passed in UDF registry.
   */
  explicit PlanState(udf::Registry* func_registry) : func_registry_(func_registry) {}

  udf::Registry* func_registry() const { return func_registry_; }

 private:
  udf::Registry* func_registry_;
};

}  // namespace plan
}  // namespace carnot
}  // namespace px
