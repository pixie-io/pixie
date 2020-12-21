#pragma once
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/probes/probes.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {
class ConfigModule : public QLObject {
 public:
  // Constant for the modules.
  inline static constexpr char kConfigModuleObjName[] = "pxconfig";
  static constexpr TypeDescriptor PxConfigModuleType = {
      /* name */ kConfigModuleObjName,
      /* type */ QLObjectType::kModule,
  };
  static StatusOr<std::shared_ptr<ConfigModule>> Create(MutationsIR* mutations_ir,
                                                        ASTVisitor* ast_visitor);

  // Constants for functions of pxtrace.
  inline static constexpr char kSetAgentConfigOpID[] = "set_agent_config";
  inline static constexpr char kSetAgentConfigOpDocstring[] = R"doc(
  Sets a particular value for a the key in the config on a particular agent.

  Args:
    agent_pod_name (int): The pod name of the Pixie agent to target for the config update.
    key (str): The key of the config to set. Can specify nested attributes as so
      `toplevel.midlevel.desired_key`.
    value (str): The value to set for the corresponding key.

  )doc";

 protected:
  explicit ConfigModule(MutationsIR* mutations_ir, ASTVisitor* ast_visitor)
      : QLObject(PxConfigModuleType, ast_visitor), mutations_ir_(mutations_ir) {}
  Status Init();

 private:
  MutationsIR* mutations_ir_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
