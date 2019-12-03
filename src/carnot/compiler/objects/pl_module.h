#pragma once
#include <memory>
#include <string>

#include "src/carnot/compiler/compiler_state/compiler_state.h"
#include "src/carnot/compiler/objects/funcobject.h"

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

  // Constants for operators in the query language.
  inline static constexpr char kDataframeOpId[] = "dataframe";
  inline static constexpr char kDisplayOpId[] = "display";

 protected:
  explicit PLModule(IR* graph, CompilerState* compiler_state)
      : QLObject(PLModuleType), graph_(graph), compiler_state_(compiler_state) {}
  Status Init();

  StatusOr<std::shared_ptr<QLObject>> GetAttributeImpl(const pypa::AstPtr& ast,
                                                       const std::string& name) const override;

 private:
  IR* graph_;
  CompilerState* compiler_state_;
};

/**
 * @brief Implements the pl.dataframe() logic
 *
 */
class DataFrameHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(IR* graph, const pypa::AstPtr& ast, const ParsedArgs& args);
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
