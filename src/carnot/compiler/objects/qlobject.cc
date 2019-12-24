#include "src/carnot/compiler/objects/qlobject.h"

#include <vector>

#include "src/carnot/compiler/objects/funcobject.h"

namespace pl {
namespace carnot {
namespace compiler {
void QLObject::AddSubscriptMethod(std::shared_ptr<FuncObject> func_object) {
  DCHECK_EQ(func_object->name(), kSubscriptMethodName);
  DCHECK(func_object->arguments() == std::vector<std::string>{"key"})
      << absl::StrJoin(func_object->arguments(), ",");
  AddMethod(kSubscriptMethodName, func_object);
}

StatusOr<std::shared_ptr<QLObject>> QLObject::GetAttribute(const pypa::AstPtr& ast,
                                                           std::string_view name) const {
  if (HasMethod(name)) {
    return GetMethod(name);
  }
  if (!HasNonMethodAttribute(name)) {
    return CreateAstError(ast, "'$0' object has no attribute '$1'", type_descriptor_.name(), name);
  }
  return GetAttributeImpl(ast, name);
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
