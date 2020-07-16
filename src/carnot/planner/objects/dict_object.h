#pragma once
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/objects/qlobject.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

class DictObject : public QLObject {
 public:
  static constexpr TypeDescriptor DictType = {
      /* name */ "dict",
      /* type */ QLObjectType::kDict,
  };

  static bool IsDict(const QLObjectPtr& object) { return object->type() == DictType.type(); }

  static StatusOr<std::shared_ptr<DictObject>> Create(const std::vector<QLObjectPtr>& keys,
                                                      const std::vector<QLObjectPtr>& values,
                                                      ASTVisitor* visitor) {
    return std::shared_ptr<DictObject>(new DictObject(keys, values, visitor));
  }

  const std::vector<QLObjectPtr>& keys() const { return *keys_; }
  const std::vector<QLObjectPtr>& values() const { return *values_; }

 protected:
  DictObject(const std::vector<QLObjectPtr>& keys, const std::vector<QLObjectPtr>& values,
             ASTVisitor* visitor)
      : QLObject(DictType, visitor) {
    keys_ = std::make_shared<std::vector<QLObjectPtr>>(keys);
    values_ = std::make_shared<std::vector<QLObjectPtr>>(values);
  }

 private:
  std::shared_ptr<std::vector<QLObjectPtr>> keys_;
  std::shared_ptr<std::vector<QLObjectPtr>> values_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
