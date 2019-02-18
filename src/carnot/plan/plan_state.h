#pragma once

#include <memory>
#include "src/carnot/udf/registry.h"

namespace pl {
namespace carnot {
namespace plan {

class PlanState {
 public:
  /**
   * Init with a UDF registry and hold a shared pointer for it.
   *
   * @param udf_registry the passed in UDF registry.
   */
  explicit PlanState(udf::ScalarUDFRegistry* udf_registry, udf::UDARegistry* uda_registry)
      : udf_registry_(udf_registry), uda_registry_(uda_registry) {}

  udf::ScalarUDFRegistry* udf_registry() const { return udf_registry_; }
  udf::UDARegistry* uda_registry() const { return uda_registry_; }

 private:
  udf::ScalarUDFRegistry* udf_registry_;
  udf::UDARegistry* uda_registry_;
};

}  // namespace plan
}  // namespace carnot
}  // namespace pl
