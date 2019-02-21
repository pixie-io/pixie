#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "src/carnot/compiler/registry_info.h"
#include "src/carnot/exec/row_descriptor.h"
#include "src/carnot/plan/relation.h"
#include "src/carnot/plan/schema.h"
#include "src/common/base.h"

namespace pl {
namespace carnot {
namespace compiler {

using RelationMap = std::unordered_map<std::string, plan::Relation>;
class CompilerState : public NotCopyable {
 public:
  /**
   * CompilerState manages the state needed to compile a single query. A new one will
   * be constructed for every query compiled in Carnot and it will not be reused.
   */
  explicit CompilerState(std::shared_ptr<RelationMap> relation_map,
                         compiler::RegistryInfo* registry_info)
      : relation_map_(relation_map), registry_info_(registry_info) {}

  CompilerState() = delete;

  RelationMap* relation_map() const { return relation_map_.get(); }
  compiler::RegistryInfo* registry_info() const { return registry_info_; }

 private:
  std::shared_ptr<RelationMap> relation_map_;
  compiler::RegistryInfo* registry_info_;
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
