#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "src/carnot/compiler/registry_info.h"
#include "src/carnot/exec/row_descriptor.h"
#include "src/carnot/plan/schema.h"
#include "src/common/base.h"

namespace pl {
namespace carnot {
namespace compiler {

class CompilerState : public NotCopyable {
  using TableTypesMap =
      std::unordered_map<std::string, std::unordered_map<std::string, udf::UDFDataType>>;

 public:
  /**
   * CompilerState manages the state needed to compile a single query. A new one will
   * be constructed for every query compiled in Carnot and it will not be reused.
   */
  explicit CompilerState(const TableTypesMap& table_types_lookup,
                         compiler::RegistryInfo* registry_info)
      : table_types_lookup_(table_types_lookup), registry_info_(registry_info) {}

  CompilerState() = delete;

  TableTypesMap table_types_lookup() const { return table_types_lookup_; }
  compiler::RegistryInfo* registry_info() const { return registry_info_; }

 private:
  TableTypesMap table_types_lookup_;
  compiler::RegistryInfo* registry_info_;
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
