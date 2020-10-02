#include "src/carnot/planner/objects/collection_object.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

Status CollectionObject::Init() {
  std::shared_ptr<FuncObject> subscript_fn(
      new FuncObject(kSubscriptMethodName, {"key"}, {},
                     /* has_variable_len_args */ false,
                     /* has_variable_len_kwargs */ false,
                     std::bind(&CollectionObject::SubscriptHandler, name(), items_,
                               std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                     ast_visitor()));
  AddSubscriptMethod(subscript_fn);
  return Status::OK();
}

StatusOr<QLObjectPtr> CollectionObject::SubscriptHandler(
    const std::string& name, std::shared_ptr<std::vector<QLObjectPtr>> items,
    const pypa::AstPtr& ast, const ParsedArgs& args, ASTVisitor*) {
  QLObjectPtr key = args.GetArg("key");

  if (!key->HasNode() || !Match(key->node(), Int())) {
    return CreateAstError(ast, "$0 indices must be integers, not $1", name, key->name());
  }

  PL_ASSIGN_OR_RETURN(IntIR * index_node, GetArgAs<IntIR>(ast, args, "key"));
  if (index_node->val() >= static_cast<int64_t>(items->size())) {
    return CreateAstError(ast, "$0 index out of range", name);
  }

  return (*items)[index_node->val()];
}

std::vector<QLObjectPtr> ObjectAsCollection(QLObjectPtr obj) {
  std::vector<QLObjectPtr> items;
  if (CollectionObject::IsCollection(obj)) {
    items = std::static_pointer_cast<CollectionObject>(obj)->items();
  } else {
    items.push_back(obj);
  }
  return items;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
