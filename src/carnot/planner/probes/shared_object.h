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

class SharedObject {
 public:
  SharedObject(std::string_view name, md::UPID upid) : name_(std::string(name)), upid_(upid) {}
  template <typename H>
  friend H AbslHashValue(H h, const SharedObject& c) {
    return H::combine(std::move(h), c.upid_, c.name_);
  }

  bool operator==(const SharedObject& rhs) const {
    return this->upid_ == rhs.upid_ && this->name_ == rhs.name_;
  }
  bool operator!=(const SharedObject& rhs) const { return !(*this == rhs); }

  const std::string& name() const { return name_; }
  const md::UPID& upid() const { return upid_; }

 private:
  std::string name_;
  md::UPID upid_;
};

/**
 * @brief SharedObjectTarget is the QLObject that wraps a shared object used as a target for
 * tracepoint deployments.
 *
 */
class SharedObjectTarget : public QLObject {
 public:
  static constexpr TypeDescriptor SharedObjectType = {
      /* name */ "SharedObject",
      /* type */ QLObjectType::kSharedObjectTraceTarget,
  };

  static StatusOr<std::shared_ptr<SharedObjectTarget>> Create(ASTVisitor* visitor,
                                                              const std::string& name,
                                                              const md::UPID& upid) {
    return std::shared_ptr<SharedObjectTarget>(new SharedObjectTarget(visitor, name, upid));
  }

  static bool IsSharedObject(const QLObjectPtr& ptr) {
    return ptr->type() == SharedObjectType.type();
  }
  const SharedObject& shared_object() { return shared_object_; }

 private:
  SharedObjectTarget(ASTVisitor* visitor, const std::string& name, const md::UPID& upid)
      : QLObject(SharedObjectType, visitor), shared_object_(name, upid) {}

  SharedObject shared_object_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
