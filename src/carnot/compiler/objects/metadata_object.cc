#include <memory>
#include <string>

#include "src/carnot/compiler/objects/expr_object.h"
#include "src/carnot/compiler/objects/metadata_object.h"

namespace pl {
namespace carnot {
namespace compiler {

StatusOr<std::shared_ptr<MetadataObject>> MetadataObject::Create(OperatorIR* op) {
  auto object = std::shared_ptr<MetadataObject>(new MetadataObject(op));
  PL_RETURN_IF_ERROR(object->Init());
  return object;
}

Status MetadataObject::Init() {
  std::shared_ptr<FuncObject> subscript_fn(
      new FuncObject(kSubscriptMethodName, {"key"}, {}, /* has_variable_len_args */ false,
                     /* has_variable_len_kwargs */ false,
                     std::bind(&MetadataObject::SubscriptHandler, this, std::placeholders::_1,
                               std::placeholders::_2)));
  AddSubscriptMethod(subscript_fn);
  return Status::OK();
}

StatusOr<QLObjectPtr> MetadataObject::SubscriptHandler(const pypa::AstPtr& ast,
                                                       const ParsedArgs& args) {
  IRNode* key = args.GetArg("key");
  if (!Match(key, String())) {
    return key->CreateIRNodeError("expected 'key' to be a str, got '$0'", key->type_string());
  }

  std::string key_value = static_cast<StringIR*>(key)->str();
  // Lookup the key
  IR* ir_graph = key->graph_ptr();

  // TODO(philkuz) (PL-1184) Only risk here is that we actually have a situation where parent_op_idx
  // is not 0. If a future developer finds this problem, use the op to reference as the parent. You
  // might have to rewire Columns to use OperatorIR* instead of parent_op_idx and then have an
  // analyzer rule that rewires to point to parent_op_idx instead and runs before everything else.
  PL_ASSIGN_OR_RETURN(MetadataIR * md_node,
                      ir_graph->CreateNode<MetadataIR>(ast, key_value, /*parent_op_idx*/ 0));
  return ExprObject::Create(md_node);
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
