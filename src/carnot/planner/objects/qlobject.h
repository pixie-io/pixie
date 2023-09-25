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

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <pypa/ast/ast.hh>

#include "src/carnot/planner/ast/ast_visitor.h"
#include "src/carnot/planner/ir/ast_utils.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/pattern_match.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

// Forward declaration necessary because FuncObjects are the methods in QLObject but
// are QLObjects themselves. Fully declared in "src/carnot/planner/objects/funcobject.h".
class FuncObject;

enum class QLObjectType {
  kMisc = 0,
  kDataframe,
  kFunction,
  kExpr,
  kList,
  kTuple,
  kNone,
  kPLModule,
  kMetadata,
  kVisualization,
  kType,
  kFlags,
  // General module type.
  kModule,
  kTraceModule,
  kTraceProgram,
  kDict,
  kTracingVariable,
  kProbe,
  kSharedObjectTraceTarget,
  kKProbeTraceTarget,
  kProcessTarget,
  kLabelSelectorTarget,
  kExporter,
  kOTelEndpoint,
  kOTelDataContainer,
};

std::string QLObjectTypeString(QLObjectType type);

class TypeDescriptor {
 public:
  constexpr TypeDescriptor(const std::string_view name, QLObjectType type)
      : name_(name), type_(type) {}
  const std::string_view& name() const { return name_; }
  const QLObjectType& type() const { return type_; }

 protected:
  const std::string_view name_;
  QLObjectType type_;
};

class QLObject;
// Alias for convenience.
using QLObjectPtr = std::shared_ptr<QLObject>;

class QLObject {
 public:
  virtual ~QLObject() = default;

  static StatusOr<QLObjectPtr> FromIRNode(CompilerState* compiler_state, IRNode* node,
                                          ASTVisitor* ast_visitor);

  /**
   * @brief Gets the Method with specified name.
   *
   * @param name the method to grab
   * @return ptr to the method. nullptr if not found.
   */
  StatusOr<std::shared_ptr<FuncObject>> GetMethod(std::string_view name) const {
    if (!methods_.contains(name)) {
      return CreateError("'$0' object has no attribute '$1'", type_descriptor_.name(), name);
    }
    return methods_.find(name)->second;
  }

  /**
   * @brief Gets the method that runs when the instantiated object is Called Directly
   * ie
   * ```
   * df = px.DataFrame(...)
   * # dataframe object called
   * df()
   * ```
   *
   * @return StatusOr<std::shared_ptr<FuncObject>>
   */
  StatusOr<std::shared_ptr<FuncObject>> GetCallMethod() const {
    if (!HasMethod(kCallMethodName)) {
      return CreateError("'$0' object is not callable", type_descriptor_.name());
    }
    return GetMethod(kCallMethodName);
  }

  /**
   * @brief Get the method that runs when this object is called with a subscript.
   * ie
   * ```
   * df = px.DataFrame(...)
   * a = dataframe[12 == 2]
   * ```
   *
   * @return StatusOr<std::shared_ptr<FuncObject>>
   */
  StatusOr<std::shared_ptr<FuncObject>> GetSubscriptMethod() const {
    if (!HasMethod(kSubscriptMethodName)) {
      return CreateError("'$0' object is not subscriptable", type_descriptor_.name());
    }
    return GetMethod(kSubscriptMethodName);
  }

  /**
   * @brief Returns whether object has a method with `name`
   *
   * @param name the string name of the method.
   * @return whether the object has the method.
   */
  bool HasMethod(std::string_view name) const { return methods_.find(name) != methods_.end(); }

  bool HasSubscriptMethod() const { return HasMethod(kSubscriptMethodName); }
  bool HasCallMethod() const { return HasMethod(kCallMethodName); }

  virtual bool CanAssignAttribute(std::string_view /*attr_name*/) const { return true; }
  Status AssignAttribute(std::string_view attr_name, QLObjectPtr object);

  StatusOr<std::shared_ptr<QLObject>> GetAttribute(const pypa::AstPtr& ast,
                                                   std::string_view attr_name) const;
  bool HasAttribute(std::string_view name) const {
    return HasNonMethodAttribute(name) || HasMethod(name);
  }

  /**
   * @brief Returns the name of all attributes that are not methods.
   *
   * @return absl::flat_hash_set<std::string> the set of all attributes.
   */
  absl::flat_hash_set<std::string> AllAttributes() const {
    absl::flat_hash_set<std::string> attrs;
    for (const auto& [k, v] : attributes_) {
      PX_UNUSED(v);
      attrs.insert(k);
    }
    return attrs;
  }

  const TypeDescriptor& type_descriptor() const { return type_descriptor_; }
  virtual std::string name() const { return std::string(type_descriptor_.name()); }
  QLObjectType type() const { return type_descriptor_.type(); }

  /**
   * @brief Returns whether this Object contains a valid node.
   *
   * @return the Node is not null.
   */

  bool HasAstPtr() const { return ast_ != nullptr; }

  /**
   * @brief Creates an error for this objects. Packages checks to make sure you have an ir node for
   * line,col error resporting. Defaults to standard error in case an ir node is nonexistant..
   *
   * @return Status
   */
  template <typename... Args>
  Status CreateError(Args... args) const {
    DCHECK(HasAstPtr());
    if (HasAstPtr()) {
      return CreateAstError(ast_, args...);
    }
    return error::InvalidArgument(args...);
  }

  /**
   * @brief Sets the docstring of this object, but doesn't add the property.
   *
   * PP-2142 needed to get around the fact that IR nodes are necessary to
   * make QLObject strings.
   *
   * @param doc_string
   * @return Status
   */
  Status SetDocString(const std::string& doc_string);

  const std::string& doc_string() const { return doc_string_; }

  // Methods are all of the methods available. Exposed to make testing easier.
  const absl::flat_hash_map<std::string, std::shared_ptr<FuncObject>>& methods() const {
    return methods_;
  }

  const absl::flat_hash_map<std::string, QLObjectPtr>& attributes() const { return attributes_; }

  void SetAst(pypa::AstPtr ast) { ast_ = std::move(ast); }

 protected:
  /**
   * @brief Construct a new QLObject. The type_descriptor must be a static member of the class.
   *
   *
   * @param type_descriptor the type descriptor
   * @param node the node to store in the QLObject. Can be null if not necessary for the
   * implementation of the QLObject.
   */
  QLObject(const TypeDescriptor& type_descriptor, pypa::AstPtr ast, ASTVisitor* ast_visitor)
      : type_descriptor_(type_descriptor), ast_(std::move(ast)), ast_visitor_(ast_visitor) {}

  QLObject(const TypeDescriptor& type_descriptor, ASTVisitor* ast_visitor)
      : QLObject(type_descriptor, nullptr, ast_visitor) {}

  /**
   * @brief Adds a method to the object. Used by QLObject derived classes to define methods.
   *
   * @param name name to reference for the method.
   * @param func_object the function object that represents the Method.
   */
  void AddMethod(const std::string& name, std::shared_ptr<FuncObject> func_object) {
    DCHECK(!HasMethod(name)) << "already exists.";
    methods_[name] = func_object;
  }

  /**
   * @brief Defines a call method for the object.
   *
   * @param func_object the func to set as the call method.
   */
  void AddCallMethod(std::shared_ptr<FuncObject> func_object) {
    AddMethod(kCallMethodName, func_object);
  }

  void AddSubscriptMethod(std::shared_ptr<FuncObject> func_object);

  /**
   * @brief Returns true if this Object has an attribute that's not a method.
   *
   * @param name the attribute name.
   * @return true if object does have attribute.
   * @return false if object does not have attribute.
   */
  virtual bool HasNonMethodAttribute(std::string_view name) const {
    return attributes_.contains(name);
  }

  /**
   * @brief nvi for GetAttributeImpl. Necessary so that dataframes, which have any value
   * as a possible attribute (because we don't know their column names) are able to override
   * and implement their own.
   *
   */
  virtual StatusOr<std::shared_ptr<QLObject>> GetAttributeImpl(const pypa::AstPtr& ast,
                                                               std::string_view attr_name) const {
    DCHECK(HasNonMethodAttribute(attr_name));
    if (!attributes_.contains(attr_name)) {
      return CreateAstError(ast, "$0 does not contain attribute '$1'", type_descriptor_.name(),
                            attr_name);
    }
    return attributes_.at(attr_name);
  }

  // Reserved keyword for call.
  inline static constexpr char kCallMethodName[] = "__call__";
  inline static constexpr char kDocStringAttributeName[] = "__doc__";
  inline static constexpr char kSubscriptMethodName[] = "__getitem__";

  ASTVisitor* ast_visitor() const { return ast_visitor_; }

  // The doc string of this object. Every object in Python has a doc string and documentation
  // implementations are much simpler if we can make that assumption here.
  std::string doc_string_;

 private:
  StatusOr<std::shared_ptr<QLObject>> GetAttributeInternal(const pypa::AstPtr& ast,
                                                           std::string_view attr_name) const;

  absl::flat_hash_map<std::string, std::shared_ptr<FuncObject>> methods_;
  absl::flat_hash_map<std::string, QLObjectPtr> attributes_;

  TypeDescriptor type_descriptor_;
  pypa::AstPtr ast_ = nullptr;
  ASTVisitor* ast_visitor_ = nullptr;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
