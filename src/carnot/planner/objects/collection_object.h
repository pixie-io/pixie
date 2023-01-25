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
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/objects/qlobject.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

class CollectionObject : public QLObject {
 public:
  const std::vector<QLObjectPtr>& items() const { return *items_; }
  static bool IsCollection(const QLObjectPtr& obj) {
    return obj->type() == QLObjectType::kList || obj->type() == QLObjectType::kTuple;
  }

 protected:
  CollectionObject(const pypa::AstPtr& ast, const std::vector<QLObjectPtr>& items,
                   const TypeDescriptor& td, ASTVisitor* visitor)
      : QLObject(td, ast, visitor) {
    items_ = std::make_shared<std::vector<QLObjectPtr>>(items);
  }

  virtual Status Init();

  /**
   * @brief Handles the GetItem calls of Collection objects.
   */
  static StatusOr<QLObjectPtr> SubscriptHandler(const std::string& name,
                                                std::shared_ptr<std::vector<QLObjectPtr>> items,
                                                const pypa::AstPtr& ast, const ParsedArgs& args,
                                                ASTVisitor* visitor);

  std::shared_ptr<std::vector<QLObjectPtr>> items_;
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

  static StatusOr<std::shared_ptr<TupleObject>> Create(const pypa::AstPtr& ast,
                                                       const std::vector<QLObjectPtr>& items,
                                                       ASTVisitor* visitor) {
    auto tuple = std::shared_ptr<TupleObject>(new TupleObject(ast, items, visitor));
    PX_RETURN_IF_ERROR(tuple->Init());
    return tuple;
  }

 protected:
  TupleObject(const pypa::AstPtr& ast, const std::vector<QLObjectPtr>& items, ASTVisitor* visitor)
      : CollectionObject(ast, items, TupleType, visitor) {}
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

  static StatusOr<std::shared_ptr<ListObject>> Create(const pypa::AstPtr& ast,
                                                      const std::vector<QLObjectPtr>& items,
                                                      ASTVisitor* visitor) {
    auto list = std::shared_ptr<ListObject>(new ListObject(ast, items, visitor));
    PX_RETURN_IF_ERROR(list->Init());
    return list;
  }

 protected:
  ListObject(const pypa::AstPtr& ast, const std::vector<QLObjectPtr>& items, ASTVisitor* visitor)
      : CollectionObject(ast, items, ListType, visitor) {}
};

/**
 * @brief ObjectAsCollection will return a vector of objects that this object represents. If the
 * object argument is a Collection, it'll return the children of the collection. Otherwise it will
 * return a vector with that object as the only element.
 *
 * Used to support function arguments that can take in either a single object or a collection.
 * @param obj The Collection or regular object.
 * @return std::vector<QLObjectPtr>
 */
std::vector<QLObjectPtr> ObjectAsCollection(QLObjectPtr obj);

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
