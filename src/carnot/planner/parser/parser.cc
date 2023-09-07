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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <magic_enum.hpp>
#include <pypa/ast/ast.hh>
#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/planner/compilerpb/compiler_status.pb.h"
#include "src/carnot/planner/parser/parser.h"
#include "src/carnot/planner/parser/string_reader.h"
#include "src/common/base/statusor.h"

namespace px {
namespace carnot {
namespace planner {

class PypaErrorHandler {
 public:
  /**
   * @brief The call back function to the error handler.
   *
   * @param err
   */
  void HandlerFunc(const pypa::Error& err) { errs_.push_back(err); }

  /**
   *
   * @brief Returns the errors as a status that can then be read by dependent functions.
   *
   * @return Status
   */
  Status ProcessErrors() {
    compilerpb::CompilerErrorGroup error_group;
    std::string msg = "";
    for (const auto& err : errs_) {
      compilerpb::CompilerError* err_pb = error_group.add_errors();
      compilerpb::LineColError* lc_err_pb = err_pb->mutable_line_col_error();
      CreateLineColError(lc_err_pb, err);
      absl::StrAppend(&msg, err.message);
    }
    return Status(statuspb::INVALID_ARGUMENT, msg,
                  std::make_unique<compilerpb::CompilerErrorGroup>(error_group));
  }

  bool HasErrors() { return errs_.size() > 0; }

 private:
  void CreateLineColError(compilerpb::LineColError* line_col_err_pb, const pypa::Error& err) {
    int64_t line = err.cur.line;
    int64_t column = err.cur.column;
    std::string error_name = absl::StrCat(magic_enum::enum_name(err.type), ":");
    std::string message = absl::Substitute("$0 $1", error_name, err.message);

    line_col_err_pb->set_line(line);
    line_col_err_pb->set_column(column);
    line_col_err_pb->set_message(message);
  }

  std::vector<pypa::Error> errs_;
};

StatusOr<pypa::AstModulePtr> Parser::Parse(std::string_view query, bool parse_doc_strings) {
  if (query.empty()) {
    return error::InvalidArgument("Query should not be empty.");
  }

  PypaErrorHandler pypa_error_handler;
  pypa::AstModulePtr ast;
  pypa::SymbolTablePtr symbols;
  pypa::ParserOptions options;

  options.docstrings = parse_doc_strings;
  options.printerrors = false;
  options.error_handler =
      std::bind(&PypaErrorHandler::HandlerFunc, &pypa_error_handler, std::placeholders::_1);
  pypa::Lexer lexer(std::make_unique<StringReader>(query));
  lexer.set_ignore_altindent_errors(false);

  pypa::parse(lexer, ast, symbols, options);
  if (pypa_error_handler.HasErrors()) {
    return pypa_error_handler.ProcessErrors();
  }
  return ast;
}

}  // namespace planner
}  // namespace carnot
}  // namespace px
