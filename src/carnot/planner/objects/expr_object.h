#pragma once
#include <memory>
#include <string>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/objects/qlobject.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief The ExprObject is a QLObject that is a container for an ExpressionIR.
 *
 * The purpose of this is to use the Object model to assemble arbitrary expressions.
 */
class ExprObject : public QLObject {
 public:
  static constexpr TypeDescriptor ExprType = {
      /* name */ "Expression",
      /* type */ QLObjectType::kExpr,
  };

  static StatusOr<std::shared_ptr<ExprObject>> Create(ExpressionIR* expr, ASTVisitor* visitor) {
    return std::shared_ptr<ExprObject>(new ExprObject(expr, visitor));
  }

  std::string name() const override {
    if (node()->type() != IRNodeType::kFunc) {
      return node()->type_string();
    }
    return "FuncCall";
  }
  static bool IsExprObject(const QLObjectPtr& object) {
    return object->type() == QLObjectType::kExpr;
  }

 protected:
  ExprObject(ExpressionIR* expr, ASTVisitor* visitor) : QLObject(ExprType, expr, visitor) {}
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
