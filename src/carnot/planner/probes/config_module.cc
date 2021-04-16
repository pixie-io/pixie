#include "src/carnot/planner/probes/config_module.h"
#include <sole.hpp>

#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/none_object.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

StatusOr<std::shared_ptr<ConfigModule>> ConfigModule::Create(MutationsIR* mutations_ir,
                                                             ASTVisitor* ast_visitor) {
  auto config_module = std::shared_ptr<ConfigModule>(new ConfigModule(mutations_ir, ast_visitor));
  PL_RETURN_IF_ERROR(config_module->Init());
  return config_module;
}
StatusOr<QLObjectPtr> SetHandler(MutationsIR* mutations, const pypa::AstPtr& ast,
                                 const ParsedArgs& args, ASTVisitor* visitor) {
  PL_ASSIGN_OR_RETURN(auto agent_pod_name_ir, GetArgAs<StringIR>(ast, args, "agent_pod_name"));
  PL_ASSIGN_OR_RETURN(auto key_ir, GetArgAs<StringIR>(ast, args, "key"));
  PL_ASSIGN_OR_RETURN(auto val_ir, GetArgAs<StringIR>(ast, args, "value"));
  mutations->AddConfig(agent_pod_name_ir->str(), key_ir->str(), val_ir->str());
  return std::static_pointer_cast<QLObject>(std::make_shared<NoneObject>(ast, visitor));
}

Status ConfigModule::Init() {
  PL_ASSIGN_OR_RETURN(
      std::shared_ptr<FuncObject> set_fn,
      FuncObject::Create(kSetAgentConfigOpID, {"agent_pod_name", "key", "value"}, {},
                         /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(SetHandler, mutations_ir_, std::placeholders::_1,
                                   std::placeholders::_2, std::placeholders::_3),
                         ast_visitor()));
  PL_RETURN_IF_ERROR(set_fn->SetDocString(kSetAgentConfigOpDocstring));
  AddMethod(kSetAgentConfigOpID, set_fn);

  return Status::OK();
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
