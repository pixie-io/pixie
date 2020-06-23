#pragma once
#include <memory>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/strings/str_join.h>
#include <pypa/ast/ast.hh>

#include "src/carnot/planner/ir/ir_nodes.h"
#include "src/carnot/planner/objects/qlobject.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief NoneObject represents None in python, the "null" object. This is used as a proxy for void
 * return type in Python interpretation.
 *
 */
class TypeObject : public QLObject {
 public:
  static constexpr TypeDescriptor TypeObjectType = {
      /* name */ "Type",
      /* type */ QLObjectType::kType,
  };

  static StatusOr<std::shared_ptr<TypeObject>> Create(IRNodeType node_type,
                                                      ASTVisitor* ast_visitor) {
    PL_ASSIGN_OR_RETURN(types::DataType data_type, IRNodeTypeToDataType(node_type));
    return Create(data_type, ast_visitor);
  }
  static StatusOr<std::shared_ptr<TypeObject>> Create(types::DataType data_type,
                                                      ASTVisitor* ast_visitor) {
    return Create(data_type, types::ST_NONE, ast_visitor);
  }
  static StatusOr<std::shared_ptr<TypeObject>> Create(types::DataType data_type,
                                                      types::SemanticType semantic_type,
                                                      ASTVisitor* ast_visitor) {
    auto type = std::shared_ptr<TypeObject>(new TypeObject(data_type, semantic_type, ast_visitor));
    PL_RETURN_IF_ERROR(type->Init());
    return type;
  }

  Status Init();

  Status NodeMatches(ExpressionIR* node) {
    // TODO(philkuz) make this nvi and expand it more.
    // TODO(philkuz) need to consider how semantic args should work in this case. Might need to add
    // specification to make args semantic args somehow.
    if (node->EvaluatedDataType() != data_type_) {
      return node->CreateIRNodeError(
          "Expected '$0', received '$1'", absl::AsciiStrToLower(magic_enum::enum_name(data_type_)),
          absl::AsciiStrToLower(magic_enum::enum_name(node->EvaluatedDataType())));
    }
    return Status::OK();
  }

  types::DataType data_type() { return data_type_; }
  types::SemanticType semantic_type() { return semantic_type_; }
  std::string TypeString() {
    if (semantic_type_ == types::ST_NONE) {
      return absl::AsciiStrToLower(magic_enum::enum_name(data_type_));
    }
    return absl::AsciiStrToLower(magic_enum::enum_name(semantic_type_));
  }

 protected:
  /**
   * @brief Construct a Type object that represents the null value in Python.
   *
   * @param ast the ast ptr for the
   */
  TypeObject(types::DataType data_type, types::SemanticType semantic_type, ASTVisitor* ast_visitor)
      : QLObject(TypeObjectType, ast_visitor),
        data_type_(data_type),
        semantic_type_(semantic_type) {}

 private:
  types::DataType data_type_;
  types::SemanticType semantic_type_;
};  // namespace compiler

class ParsedArgs;

/**
 * @brief Implements the cast logic
 */
class CastHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(types::DataType data_type, types::SemanticType semantic_type,
                                    const pypa::AstPtr& ast, const ParsedArgs& args,
                                    ASTVisitor* visitor);
};
}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
