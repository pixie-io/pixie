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
  explicit PlanState(std::shared_ptr<udf::ScalarUDFRegistry> udf_registry,
                     std::shared_ptr<udf::UDARegistry> uda_registry)
      : udf_registry_(udf_registry), uda_registry_(uda_registry) {}

  std::shared_ptr<udf::ScalarUDFRegistry> udf_registry() const { return udf_registry_; }
  std::shared_ptr<udf::UDARegistry> uda_registry() const { return uda_registry_; }

 private:
  std::shared_ptr<udf::ScalarUDFRegistry> udf_registry_;
  std::shared_ptr<udf::UDARegistry> uda_registry_;
};

}  // namespace plan
}  // namespace carnot
}  // namespace pl
