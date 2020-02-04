#pragma once
#include <memory>
#include <string>

#include "src/carnot/compiler/compiler_state/compiler_state.h"
#include "src/carnot/compiler/objects/qlobject.h"

namespace pl {
namespace carnot {
namespace compiler {

/**
 * @brief The ExprObject is a QLObject that is a container for an ExpressionIR.
 *
 * The purpose of this is to use the Object model to assemble arbitrary expressions.
 */
class ExprObject : public QLObject {
 public:
  static constexpr TypeDescriptor ExprType = {
      /* name */ "expression",
      /* type */ QLObjectType::kExpr,
  };

  static StatusOr<std::shared_ptr<ExprObject>> Create(ExpressionIR* expr) {
    return std::shared_ptr<ExprObject>(new ExprObject(expr));
  }

 protected:
  explicit ExprObject(ExpressionIR* expr) : QLObject(ExprType, expr) {}
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
