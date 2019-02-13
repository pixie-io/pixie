#pragma once

#include <memory>
#include "src/carnot/compiler/registry_info.h"
#include "src/carnot/plan/schema.h"

namespace pl {
namespace carnot {
namespace compiler {

class CompilerState {
 public:
  /**
   * CompilerState manages the state needed to compile a single query. A new one will
   * be constructed for every query compiled in Carnot and it will not be reused.
   */
  explicit CompilerState(std::shared_ptr<plan::Schema> schema,
                         std::shared_ptr<compiler::RegistryInfo> registry_info)
      : schema_(schema), registry_info_(registry_info) {}

  std::shared_ptr<plan::Schema> schema() const { return schema_; }
  std::shared_ptr<compiler::RegistryInfo> registry_info() const { return registry_info_; }

 private:
  std::shared_ptr<plan::Schema> schema_;
  std::shared_ptr<compiler::RegistryInfo> registry_info_;
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
