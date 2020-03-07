#pragma once
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/objects/qlobject.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

class CollectionObject : public QLObject {
 public:
  const std::vector<QLObjectPtr>& items() const { return items_; }
  static bool IsCollection(QLObjectPtr obj) {
    return obj->type() == QLObjectType::kList || obj->type() == QLObjectType::kTuple;
  }

 protected:
  CollectionObject(const std::vector<QLObjectPtr>& items, const TypeDescriptor& td,
                   ASTVisitor* visitor)
      : QLObject(td, visitor), items_(items) {}
  std::vector<QLObjectPtr> items_;
};

/**
 * @brief Contains a tuple of QLObjects
 */
class TupleObject : public CollectionObject {
 public:
  static constexpr TypeDescriptor TupleType = {
      /* name */ "tuple",
      /* type */ QLObjectType::kTuple,
  };

  static StatusOr<std::shared_ptr<TupleObject>> Create(const std::vector<QLObjectPtr>& items,
                                                       ASTVisitor* visitor) {
    return std::shared_ptr<TupleObject>(new TupleObject(items, visitor));
  }

 protected:
  TupleObject(const std::vector<QLObjectPtr>& items, ASTVisitor* visitor)
      : CollectionObject(items, TupleType, visitor) {}
};

/**
 * @brief Contains a list of QLObjects
 */
class ListObject : public CollectionObject {
 public:
  static constexpr TypeDescriptor ListType = {
      /* name */ "list",
      /* type */ QLObjectType::kList,
  };

  static StatusOr<std::shared_ptr<ListObject>> Create(const std::vector<QLObjectPtr>& items,
                                                      ASTVisitor* visitor) {
    return std::shared_ptr<ListObject>(new ListObject(items, visitor));
  }

 protected:
  ListObject(const std::vector<QLObjectPtr>& items, ASTVisitor* visitor)
      : CollectionObject(items, ListType, visitor) {}
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
