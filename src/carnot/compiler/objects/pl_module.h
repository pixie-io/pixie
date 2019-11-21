#pragma once
#include <memory>
#include <string>

#include "src/carnot/compiler/compiler_state/compiler_state.h"
#include "src/carnot/compiler/objects/qlobject.h"

namespace pl {
namespace carnot {
namespace compiler {

class PLModule : public QLObject {
 public:
  static constexpr TypeDescriptor PLModuleType = {
      /* name */ "pl",
      /* type */ QLObjectType::kPLModule,
  };
  static StatusOr<std::shared_ptr<PLModule>> Create(IR* graph, CompilerState* compiler_state);

 protected:
  explicit PLModule(IR* graph, CompilerState* compiler_state)
      : QLObject(PLModuleType), graph_(graph), compiler_state_(compiler_state) {}
  Status Init();

  StatusOr<std::shared_ptr<QLObject>> GetAttributeImpl(const pypa::AstPtr& ast,
                                                       const std::string& name) const override;
  bool HasAttributeImpl(const std::string& name) const override;

 private:
  IR* graph_;
  CompilerState* compiler_state_;
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
