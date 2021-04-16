#include "src/carnot/planner/objects/module.h"
#include "src/carnot/planner/parser/parser.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<std::shared_ptr<Module>> Module::Create(std::string_view module_text,
                                                 ASTVisitor* visitor) {
  std::shared_ptr<Module> module(new Module(visitor));
  PL_RETURN_IF_ERROR(module->Init(module_text));
  return module;
}

StatusOr<std::shared_ptr<QLObject>> Module::GetAttributeImpl(const pypa::AstPtr& ast,
                                                             std::string_view name) const {
  if (!var_table_->HasVariable(name)) {
    return CreateAstError(ast, "No such attribute '$0'", name);
  }
  return var_table_->Lookup(name);
}

Status Module::Init(std::string_view module_text) {
  Parser parser;
  PL_ASSIGN_OR_RETURN(pypa::AstModulePtr ast,
                      parser.Parse(module_text.data(), /* parse_doc_strings */ true));
  var_table_ = VarTable::Create();
  module_visitor_ = ast_visitor()->CreateModuleVisitor(var_table_);
  PL_RETURN_IF_ERROR(module_visitor_->ProcessModuleNode(ast));
  for (const auto& [var, obj] : var_table_->scope_table()) {
    if (obj->type() == QLObjectType::kFunction) {
      AddMethod(var, std::static_pointer_cast<FuncObject>(obj));
    }
  }
  return Status::OK();
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
