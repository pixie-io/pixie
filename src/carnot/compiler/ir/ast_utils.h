#pragma once
#include <memory>
#include <string>
#include <vector>

#include <pypa/ast/ast.hh>

#include "src/carnot/compiler/compiler_error_context/compiler_error_context.h"
#include "src/carnot/compiler/compilerpb/compiler_status.pb.h"
#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/ir/pattern_match.h"
#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/common/base/statusor.h"

namespace pl {
namespace carnot {
namespace compiler {

#define PYPA_PTR_CAST(TYPE, VAL) \
  std::static_pointer_cast<typename pypa::AstTypeByID<pypa::AstType::TYPE>::Type>(VAL)

#define PYPA_CAST(TYPE, VAL) static_cast<AstTypeByID<AstType::TYPE>::Type&>(VAL)

/**
 * @brief Create an error that incorporates line, column of ast node into the error message.
 * @param ast the ast value to use to create an error.
 * @param err_msg the error message to store.
 * @return Status the formatted error
 */
template <typename... Args>
Status CreateAstError(const pypa::AstPtr& ast, Args... args) {
  compilerpb::CompilerErrorGroup context =
      LineColErrorPb(ast->line, ast->column, absl::Substitute(args...));
  return Status(statuspb::INVALID_ARGUMENT, "",
                std::make_unique<compilerpb::CompilerErrorGroup>(context));
}

template <typename... Args>
Status CreateAstError(const pypa::Ast& ast, Args... args) {
  return CreateAstError(std::make_shared<pypa::Ast>(ast), args...);
}

/**
 * @brief Parses a list as a string list. If any elements are not strings then it will fail.
 *
 * @param list_ir
 * @return StatusOr<std::vector<std::string>>
 */
StatusOr<std::vector<std::string>> ParseStringsFromCollection(const CollectionIR* list_ir);

/**
 * @brief Get the string repr of the passed in type.
 *
 * @param type
 * @return std::string
 */
std::string GetAstTypeName(pypa::AstType type);

/**
 * @brief Get the Id from the NameAST.
 *
 * @param node
 * @return std::string
 */
std::string GetNameAsString(const pypa::AstPtr& node);

/**
 * @brief Gets the string out of what is suspected to be a strAst. Errors out if ast is not of
 * type str.
 *
 * @param ast
 * @return StatusOr<std::string>
 */
StatusOr<std::string> GetStrAstValue(const pypa::AstPtr& ast);

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
