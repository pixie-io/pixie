#pragma once
#include <memory>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <pypa/ast/ast.hh>

#include "src/carnot/planner/objects/collection_object.h"
#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/objects/var_table.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief Module is used to load data from "external files" to be available as imports. Attempting
 * to be identical in usage to Python modules.
 */
class Module : public QLObject {
 public:
  static constexpr TypeDescriptor ModuleType = {
      /* name */ "Module",
      /* type */ QLObjectType::kModule,
  };
  /**
   * @brief Create will parse the module_text into an object. It should have it's own var_table
   * owned by the module, which will be exposed through the GetAttributeImpl.
   *
   * @param visitor
   * @return StatusOr<std::shared_ptr<Module>>
   */
  static StatusOr<std::shared_ptr<Module>> Create(std::string_view module_text,
                                                  ASTVisitor* visitor);

 protected:
  explicit Module(ASTVisitor* visitor) : QLObject(ModuleType, visitor) {}
  StatusOr<std::shared_ptr<QLObject>> GetAttributeImpl(const pypa::AstPtr& ast,
                                                       std::string_view name) const override;

  Status Init(std::string_view module_text);
  bool HasNonMethodAttribute(std::string_view /* name */) const override { return true; }

 private:
  std::shared_ptr<VarTable> var_table_;
  std::shared_ptr<ASTVisitor> module_visitor_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
