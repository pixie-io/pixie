#pragma once
#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include <cstdio>
#include <fstream>
#include <memory>
#include <string>

#include "src/carnot/compiler/ast_visitor.h"

namespace pl {
namespace carnot {
namespace compiler {

const std::string& GenRandomStr(const int len) {
  static std::string s(len, 'a');
  static std::string alphanum =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";

  for (int i = 0; i < len; ++i) {
    s[i] = alphanum[rand() % (alphanum.size() - 1)];
  }
  return s;
}

bool FileExists(const std::string& name) {
  struct stat buffer;
  return (stat(name.c_str(), &buffer) == 0);
}

std::string GetTempFileName(std::string tmp_dir) {
  srand(time(NULL));
  std::string tmp_filename;
  do {
    tmp_filename = absl::StrFormat("%s/%s.pl", tmp_dir, GenRandomStr(10));
  } while (FileExists(tmp_filename));
  return tmp_filename;
}
/**
 * @brief Creates a file that contains
 * the query_str.
 *
 * This is a temporary workaround to
 * work directly with built-in libpypa
 * functionality (which uses a FileBuffer)
 * rather than trying to create a string buffer.
 *
 * @param query_str
 * @return std::string filename.
 */
std::string SetupQuery(const std::string& query_str) {
  // create tmpfilename
  std::string filename = GetTempFileName("/tmp");
  // write query to tmpfile
  std::ofstream tmp_file;
  tmp_file.open(filename);
  tmp_file << query_str;
  tmp_file.close();
  // return filename
  return filename;
}
/**
 * @brief ParseFile takes in a filename
 * an passes it to libpypa.
 *
 * This is a temporary workaround to
 * work directly with built-in libpypa
 * functionality (which uses a FileBuffer)
 * rather than trying to create a string buffer.
 *
 * @param filename that contains the query to be parsed
 */
StatusOr<std::shared_ptr<IR>> ParseFile(std::string filename) {
  std::shared_ptr<IR> ir = std::make_shared<IR>();
  ASTWalker a(ir);

  pypa::AstModulePtr ast;
  pypa::SymbolTablePtr symbols;
  pypa::ParserOptions options;
  if (VLOG_IS_ON(1)) {
    options.printerrors = true;
  } else {
    options.printerrors = false;
  }
  pypa::Lexer lexer(filename.c_str());

  Status result;
  if (pypa::parse(lexer, ast, symbols, options)) {
    result = a.ProcessModuleNode(ast);
  } else {
    result = error::InvalidArgument("Parsing was unsuccessful, likely because of broken argument.");
  }
  PL_RETURN_IF_ERROR(result);
  VLOG(2) << ir->DebugString() << std::endl;
  return ir;
}
/**
 * @brief Parses a query.
 *
 * Has some extra functionality to
 * temporarily use libpypa's FileBuf
 * rather than trying to roll a string buffer, which
 * might be a very costly experience.
 *
 * @param query_str
 */
StatusOr<std::shared_ptr<IR>> ParseQuery(const std::string& query_str) {
  // create file that contains queryString
  std::string filename = SetupQuery(query_str);
  // parse to AST and walk
  auto status = ParseFile(filename);
  // clean up
  std::remove(filename.c_str());
  // TODO(philkuz) figure out how to change log verbosity.
  VLOG(1) << status.status().ToString() << std::endl;
  return status;
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
