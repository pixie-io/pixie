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
StatusOr<std::vector<std::string>> ParseStringListIR(const ListIR* list_ir);

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
