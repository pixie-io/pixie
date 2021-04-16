#pragma once
#include <memory>
#include <string>

#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/objects/qlobject.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

class VisualizationObject : public QLObject {
 public:
  static constexpr TypeDescriptor VisualizationTypeDescriptor = {
      /* name */ "vis",
      /* type */ QLObjectType::kVisualization,
  };
  static StatusOr<std::shared_ptr<VisualizationObject>> Create(ASTVisitor* ast_visitor);

  // Constant for the children of the visualization object.
  inline static constexpr char kVegaAttrId[] = "vega";

 protected:
  explicit VisualizationObject(ASTVisitor* ast_visitor)
      : QLObject(VisualizationTypeDescriptor, ast_visitor) {}

  Status Init();
  IR* graph_;
};

class VegaHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(const pypa::AstPtr& ast, const ParsedArgs& args,
                                    ASTVisitor* visitor);
  static StatusOr<QLObjectPtr> NestedFn(std::string vega_spec, const pypa::AstPtr& ast,
                                        const ParsedArgs& args, ASTVisitor* visitor);
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
