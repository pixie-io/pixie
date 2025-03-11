/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/planner/objects/funcobject.h"
#include "src/shared/upid/upid.h"

namespace px {
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

  static StatusOr<std::shared_ptr<SharedObjectTarget>> Create(const pypa::AstPtr& ast,
                                                              ASTVisitor* visitor,
                                                              const std::string& name,
                                                              const md::UPID& upid) {
    return std::shared_ptr<SharedObjectTarget>(new SharedObjectTarget(ast, visitor, name, upid));
  }

  static bool IsSharedObject(const QLObjectPtr& ptr) {
    return ptr->type() == SharedObjectType.type();
  }
  const SharedObject& shared_object() { return shared_object_; }

 private:
  SharedObjectTarget(const pypa::AstPtr& ast, ASTVisitor* visitor, const std::string& name,
                     const md::UPID& upid)
      : QLObject(SharedObjectType, ast, visitor), shared_object_(name, upid) {}

  SharedObject shared_object_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
