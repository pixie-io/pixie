#include <vector>

#include "src/carnot/compiler/objects/expr_object.h"
#include "src/carnot/compiler/objects/pl_module.h"
namespace pl {
namespace carnot {
namespace compiler {

StatusOr<std::shared_ptr<PLModule>> PLModule::Create(IR* graph, CompilerState* compiler_state) {
  auto module = std::shared_ptr<PLModule>(new PLModule(graph, compiler_state));

  PL_RETURN_IF_ERROR(module->Init());
  return module;
}

Status PLModule::Init() {
  // TODO(philkuz) (PL-1189) remove this when the udf names no longer have the 'pl.' prefix.
  for (const auto& name : compiler_state_->registry_info()->func_names()) {
    attributes_.emplace(absl::StripPrefix(name, "pl."));
  }
  // TODO(philkuz) (PL-1189) enable this.
  // attributes_ = compiler_state_->registry_info()->func_names()

  return Status::OK();
}

StatusOr<QLObjectPtr> PLModule::GetAttributeImpl(const pypa::AstPtr& ast,
                                                 const std::string& name) const {
  // If this gets to this point, should fail here.
  DCHECK(HasNonMethodAttribute(name));

  PL_ASSIGN_OR_RETURN(FuncIR * func,
                      graph_->CreateNode<FuncIR>(ast, FuncIR::Op{FuncIR::Opcode::non_op, "", name},
                                                 std::vector<ExpressionIR*>{}));
  return ExprObject::Create(func);
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
