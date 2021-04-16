#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <pypa/ast/ast.hh>
#include <pypa/ast/tree_walker.hh>

#include "src/common/base/statusor.h"

namespace px {
namespace carnot {
namespace planner {

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
  StatusOr<pypa::AstModulePtr> Parse(std::string_view query, bool parse_doc_strings = true);
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
