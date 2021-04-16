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
    for (const auto& err : errs_) {
      compilerpb::CompilerError* err_pb = error_group.add_errors();
      compilerpb::LineColError* lc_err_pb = err_pb->mutable_line_col_error();
      CreateLineColError(lc_err_pb, err);
    }
    return Status(statuspb::INVALID_ARGUMENT, "",
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
