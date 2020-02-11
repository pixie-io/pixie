#include <vector>

#include "src/carnot/compiler/objects/collection_object.h"
#include "src/carnot/compiler/objects/dataframe.h"
#include "src/carnot/compiler/objects/expr_object.h"
#include "src/carnot/compiler/objects/funcobject.h"
#include "src/carnot/compiler/objects/qlobject.h"

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

Status QLObject::AssignAttribute(std::string_view attr_name, QLObjectPtr object) {
  if (!CanAssignAttribute(attr_name)) {
    return CreateError("Cannot assign attribute $0 to object of type $1", attr_name, name());
  }
  attributes_[attr_name] = object;
  return Status::OK();
}

StatusOr<QLObjectPtr> QLObject::FromIRNode(IRNode* node) {
  if (Match(node, Operator())) {
    return Dataframe::Create(static_cast<OperatorIR*>(node));
  } else if (Match(node, Collection())) {
    return CollectionObject::Create(static_cast<CollectionIR*>(node));
  } else if (Match(node, Expression())) {
    return ExprObject::Create(static_cast<ExpressionIR*>(node));
  } else {
    return node->CreateIRNodeError("Could not create QL object from IRNode of type $0",
                                   node->type_string());
  }
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
