#pragma once
#include <memory>
#include <string>

#include <pypa/ast/ast.hh>
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"

#include "src/carnot/compiler/ir/ast_utils.h"
#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/ir/pattern_match.h"

namespace pl {
namespace carnot {
namespace compiler {

// Forward declaration necessary because FuncObjects are the methods in QLObject but
// are QLObjects themselves. Fully declared in "src/carnot/compiler/objects/funcobject.h".
class FuncObject;

enum class QLObjectType { kMisc = 0, kDataframe, kFunction, kExpr, kNone, kPLModule, kMetadata };

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

class QLObject {
 public:
  virtual ~QLObject() = default;
  /**
   * @brief Gets the Method with specified name.
   *
   * @param name the method to grab
   * @return ptr to the method. nullptr if not found.
   */
  StatusOr<std::shared_ptr<FuncObject>> GetMethod(const std::string& name) const {
    if (!methods_.contains(name)) {
      return CreateError("'$0' object has no attribute '$1'", type_descriptor_.name(), name);
    }
    return methods_.find(name)->second;
  }

  /**
   * @brief Gets the method that runs when the instantiated object is Called Directly
   * ie
   * ```
   * df = pl.DataFrame(...)
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
   * df = pl.DataFrame(...)
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
  bool HasMethod(const std::string& name) const { return methods_.find(name) != methods_.end(); }

  bool HasSubscriptMethod() const { return HasMethod(kSubscriptMethodName); }
  bool HasCallMethod() const { return HasMethod(kCallMethodName); }

  StatusOr<std::shared_ptr<QLObject>> GetAttribute(const pypa::AstPtr& ast,
                                                   const std::string& name) const;

  bool HasAttribute(const std::string& name) const {
    return HasNonMethodAttribute(name) || HasMethod(name);
  }

  /**
   * @brief Returns the name of all attributes that are not methods.
   *
   * @return const absl::flat_hash_set<std::string>&  the set of all attributes.
   */
  const absl::flat_hash_set<std::string>& AllAttributes() const { return attributes_; }

  const TypeDescriptor& type_descriptor() { return type_descriptor_; }
  IRNode* node() const { return node_; }

  /**
   * @brief Returns whether this Object contains a valid node.
   *
   * @return the Node is not null.
   */
  bool HasNode() const { return node_ != nullptr; }

  bool HasAstPtr() const { return ast_ != nullptr; }

  /**
   * @brief Creates an error for this objects. Packages checks to make sure you have an ir node for
   * line,col error resporting. Defaults to standard error in case an ir node is nonexistant..
   *
   * @return Status
   */
  template <typename... Args>
  Status CreateError(Args... args) const {
    if (HasNode()) {
      return node_->CreateIRNodeError(args...);
    }
    if (HasAstPtr()) {
      return CreateAstError(ast_, args...);
    }
    return error::InvalidArgument(args...);
  }

  // Methods are all of the methods available. Exposed to make testing easier.
  const absl::flat_hash_map<std::string, std::shared_ptr<FuncObject>>& methods() const {
    return methods_;
  }

 protected:
  /**
   * @brief Construct a new QLObject. The type_descriptor must be a static member of the class.
   *
   *
   * @param type_descriptor the type descriptor
   * @param node the node to store in the QLObject. Can be null if not necessary for the
   * implementation of the QLObject.
   */
  QLObject(const TypeDescriptor& type_descriptor, IRNode* node, pypa::AstPtr ast)
      : type_descriptor_(type_descriptor), node_(node), ast_(ast) {}

  explicit QLObject(const TypeDescriptor& type_descriptor)
      : QLObject(type_descriptor, nullptr, nullptr) {}

  QLObject(const TypeDescriptor& type_descriptor, IRNode* node)
      : QLObject(type_descriptor, node, nullptr) {}

  QLObject(const TypeDescriptor& type_descriptor, pypa::AstPtr ast)
      : QLObject(type_descriptor, nullptr, ast) {}

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
   * @brief NVI for GetAttribute(). GetAttribute is not simple in our objects model. GetAttribute
   * should return an IRNode but there are cases where we don't want to return the same IRNode each
   * time GetAttribute("x") is called, instead we want to create a new IRNode.
   *
   * This excludes the possibility of holding a map of strings to IRNodes or QLObjects.
   *
   * Instead we have the children of QLObject implement this NVI so the children make the creation
   * vs maintenance choice.
   *
   * Note -> to be compatible with the AST visitor and not cause unnecessary errors, the QLObject
   * returned by must have QLObject->node() set by using one of the QLObject Constructors with an
   * IRNode* node argument when constructing their own objects.
   *
   * // TODO(reviewer) should we make this into a Pure virtual function?
   *
   * @return StatusOr<std::shared_ptr<QLObject>>
   */
  virtual StatusOr<std::shared_ptr<QLObject>> GetAttributeImpl(const pypa::AstPtr&,
                                                               const std::string&) const {
    return error::Unimplemented("");
  }

  bool HasNonMethodAttribute(std::string_view name) const { return AllAttributes().contains(name); }

  // Reserved keyword for call.
  inline static constexpr char kCallMethodName[] = "__call__";
  inline static constexpr char kSubscriptMethodName[] = "__getitem__";

  // Attributes set.
  absl::flat_hash_set<std::string> attributes_;

 private:
  absl::flat_hash_map<std::string, std::shared_ptr<FuncObject>> methods_;
  TypeDescriptor type_descriptor_;
  IRNode* node_ = nullptr;
  pypa::AstPtr ast_ = nullptr;
};

// Alias for convenience.
using QLObjectPtr = std::shared_ptr<QLObject>;

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
