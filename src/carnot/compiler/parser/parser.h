#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <pypa/ast/ast.hh>
#include <pypa/ast/tree_walker.hh>

#include "src/common/base/statusor.h"

namespace pl {
namespace carnot {
namespace compiler {

/**
 * Parser converts a query into an AST or errors out.
 */
class Parser {
 public:
  /**
   * Parses the query into an ast.
   * @param query the query to compile.
   * @return the ast module that represents the query.
   */
  StatusOr<pypa::AstModulePtr> Parse(std::string_view query);
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
