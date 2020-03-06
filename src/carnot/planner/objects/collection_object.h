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
 * @brief The CollectionObject is a QLObject that is a container for an CollectionIR.
 */
class CollectionObject : public QLObject {
 public:
  static constexpr TypeDescriptor CollectionType = {
      /* name */ "collection",
      /* type */ QLObjectType::kCollection,
  };

  static StatusOr<std::shared_ptr<CollectionObject>> Create(CollectionIR* collection,
                                                            ASTVisitor* visitor) {
    return std::shared_ptr<CollectionObject>(new CollectionObject(collection, visitor));
  }

 protected:
  CollectionObject(CollectionIR* collection, ASTVisitor* visitor)
      : QLObject(CollectionType, collection, visitor) {}
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
