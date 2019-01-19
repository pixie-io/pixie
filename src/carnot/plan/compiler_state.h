#pragma once

#include <memory>
#include "src/carnot/udf/registry.h"

namespace pl {
namespace carnot {
namespace plan {

class CompilerState {
 public:
  /**
   * Init with a UDF registry and hold a shared pointer for it.
   *
   * @param udf_registry the passed in UDF registry.
   */
  explicit CompilerState(std::shared_ptr<udf::ScalarUDFRegistry> udf_registry)
      : udf_registry_(udf_registry) {}

  std::shared_ptr<udf::ScalarUDFRegistry> udf_registry() const { return udf_registry_; }

 private:
  std::shared_ptr<udf::ScalarUDFRegistry> udf_registry_;
};

}  // namespace plan
}  // namespace carnot
}  // namespace pl
