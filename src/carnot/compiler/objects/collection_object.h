#pragma once
#include <memory>
#include <string>

#include "src/carnot/compiler/compiler_state/compiler_state.h"
#include "src/carnot/compiler/objects/qlobject.h"

namespace pl {
namespace carnot {
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

  static StatusOr<std::shared_ptr<CollectionObject>> Create(CollectionIR* collection) {
    return std::shared_ptr<CollectionObject>(new CollectionObject(collection));
  }

 protected:
  explicit CollectionObject(CollectionIR* collection) : QLObject(CollectionType, collection) {}
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
