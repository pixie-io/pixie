#pragma once

#include <gmock/gmock.h>
#include <utility>
#include <vector>

#include <cstdio>
#include <fstream>
#include <memory>
#include <string>

#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/ast_visitor.h"
#include "src/carnot/compiler/compiler_state.h"
#include "src/carnot/compiler/string_reader.h"

namespace pl {
namespace carnot {
namespace compiler {
/**
 * @brief Makes a test ast ptr that makes testing IRnode
 * Init calls w/o queries not error out.
 *
 * @return pypa::AstPtr
 */
pypa::AstPtr MakeTestAstPtr() {
  pypa::Ast ast_obj(pypa::AstType::Bool);
  ast_obj.line = 0;
  ast_obj.column = 0;
  return std::make_shared<pypa::Ast>(ast_obj);
}
/**
 * @brief Parses a query.
 *
 * @param query_str
 */
StatusOr<std::shared_ptr<IR>> ParseQuery(const std::string& query) {
  std::shared_ptr<IR> ir = std::make_shared<IR>();
  auto info = std::make_shared<RegistryInfo>();
  udfspb::UDFInfo info_pb;
  PL_RETURN_IF_ERROR(info->Init(info_pb));
  auto compiler_state =
      std::make_shared<CompilerState>(std::make_unique<RelationMap>(), info.get(), 0);
  ASTWalker ast_walker(ir, compiler_state.get());

  pypa::AstModulePtr ast;
  pypa::SymbolTablePtr symbols;
  pypa::ParserOptions options;
  pypa::Lexer lexer(std::make_unique<StringReader>(query));

  if (VLOG_IS_ON(1)) {
    options.printerrors = true;
  } else {
    options.printerrors = false;
  }

  if (pypa::parse(lexer, ast, symbols, options)) {
    PL_RETURN_IF_ERROR(ast_walker.ProcessModuleNode(ast));
  } else {
    return error::InvalidArgument("Parsing was unsuccessful, likely because of broken argument.");
  }

  VLOG(2) << ir->DebugString() << std::endl;
  return ir;
}

struct CompilerErrorMatcher {
  explicit CompilerErrorMatcher(std::string text_pb)
      : expected_compiler_error_(std::move(text_pb)) {}

  bool MatchAndExplain(const Status& status, ::testing::MatchResultListener* listener) const {
    if (status.ok()) {
      (*listener) << "Status is ok, no compiler error found.";
    }
    if (!status.has_context()) {
      (*listener) << "Status does not have a context.";
      return false;
    }
    if (!status.context()->Is<compilerpb::CompilerErrorGroup>()) {
      (*listener) << "Status context is not a CompilerErrorGroup.";
      return false;
    }
    compilerpb::CompilerErrorGroup error_group;
    if (!status.context()->UnpackTo(&error_group)) {
      (*listener) << "Couldn't unpack the error to a compiler error group.";
      return false;
    }

    if (error_group.errors_size() == 0) {
      (*listener) << "No compile errors found.";
      return false;
    }

    std::vector<std::string> error_messages;
    for (int64_t i = 0; i < error_group.errors_size(); i++) {
      auto error = error_group.errors(i).line_col_error();
      std::string msg = error.message();
      if (msg == expected_compiler_error_) {
        return true;
      }
      error_messages.push_back(msg);
    }
    (*listener) << absl::Substitute("Expected compiler error '$0' not found. Have '$1'",
                                    expected_compiler_error_, absl::StrJoin(error_messages, ","));
    return false;
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "equals message: " << expected_compiler_error_;
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "does not equal message: " << expected_compiler_error_;
  }

  std::string expected_compiler_error_;
};

template <typename... Args>
inline ::testing::PolymorphicMatcher<CompilerErrorMatcher> HasCompilerError(
    Args... substitute_args) {
  return ::testing::MakePolymorphicMatcher(
      CompilerErrorMatcher(std::move(absl::Substitute(substitute_args...))));
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
