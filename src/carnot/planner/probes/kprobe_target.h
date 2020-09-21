#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/planner/objects/funcobject.h"
#include "src/shared/metadata/base_types.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief KProbeTarget is the QLObject that wraps something that targets kprobes.
 */
class KProbeTarget : public QLObject {
 public:
  static constexpr TypeDescriptor KProbeTargetType = {
      /* name */ "kprobe",
      /* type */ QLObjectType::kKProbeTraceTarget,
  };

  static StatusOr<std::shared_ptr<KProbeTarget>> Create(ASTVisitor* visitor) {
    return std::shared_ptr<KProbeTarget>(new KProbeTarget(visitor));
  }

  static bool IsKProbeTarget(const QLObjectPtr& ptr) {
    return ptr->type() == KProbeTargetType.type();
  }

 private:
  explicit KProbeTarget(ASTVisitor* visitor) : QLObject(KProbeTargetType, visitor) {}
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
