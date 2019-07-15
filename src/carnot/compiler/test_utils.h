#pragma once

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

bool StatusHasCompilerError(const Status& status, const std::string& expected_message) {
  CHECK(status.has_context());
  CHECK(status.context()->Is<compilerpb::CompilerErrorGroup>());
  compilerpb::CompilerErrorGroup error_group;
  CHECK(status.context()->UnpackTo(&error_group));
  for (int64_t i = 0; i < error_group.errors_size(); i++) {
    if (error_group.errors(i).line_col_error().message() == expected_message) {
      return true;
    }
  }
  return false;
}

bool StatusHasCompilerError(const Status& status, const std::string& expected_message,
                            uint64_t line, uint64_t column) {
  CHECK(status.has_context());
  CHECK(status.context()->Is<compilerpb::CompilerErrorGroup>());
  compilerpb::CompilerErrorGroup error_group;
  CHECK(status.context()->UnpackTo(&error_group));
  for (int64_t i = 0; i < error_group.errors_size(); i++) {
    auto error = error_group.errors(i).line_col_error();
    if (error.message() == expected_message && error.line() == line && error.column() == column) {
      return true;
    }
  }
  return false;
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
