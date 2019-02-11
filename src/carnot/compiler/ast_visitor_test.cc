#include "src/carnot/compiler/ast_visitor.h"
#include <gtest/gtest.h>
#include <pypa/ast/tree_walker.hh>
#include <pypa/parser/parser.hh>

#include <cstdio>
#include <fstream>

#include "src/carnot/compiler/ir_test_utils.h"

namespace pl {
namespace carnot {
namespace compiler {

std::string genRandomStr(const int len) {
  std::string s(len, 'a');
  std::string alphanum =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";

  for (int i = 0; i < len; ++i) {
    s[i] = alphanum[rand() % (alphanum.size() - 1)];
  }
  return s;
}

bool fileExists(const std::string& name) {
  struct stat buffer;
  return (stat(name.c_str(), &buffer) == 0);
}

std::string getTempFileName(std::string tmpDir) {
  srand(time(NULL));
  std::string tmpFileName;
  do {
    tmpFileName = tmpDir + "/" + genRandomStr(10);
  } while (fileExists(tmpFileName));
  return tmpFileName;
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
std::string setupQuery(const std::string& query_str) {
  // create tmpfilename
  std::string filename = getTempFileName("/tmp");
  // write query to tmpfile
  std::ofstream tmp_file;
  tmp_file.open(filename);
  tmp_file << query_str;
  tmp_file.close();
  // return filename
  return filename;
}
/**
 * @brief parseFile takes in a filename
 * an passes it to libpypa.
 *
 * This is a temporary workaround to
 * work directly with built-in libpypa
 * functionality (which uses a FileBuffer)
 * rather than trying to create a string buffer.
 *
 * @param filename that contains the query to be parsed
 */
StatusOr<std::shared_ptr<IR>> parseFile(std::string filename) {
  std::shared_ptr<IR> ir = std::make_shared<IR>();
  ASTWalker a(ir);

  pypa::AstModulePtr ast;
  pypa::SymbolTablePtr symbols;
  pypa::ParserOptions options;
  options.printerrors = false;
  options.printdbgerrors = false;
  pypa::Lexer lexer(filename.c_str());

  Status result;
  if (pypa::parse(lexer, ast, symbols, options)) {
    result = a.ProcessModuleNode(ast);
  } else {
    result = error::InvalidArgument("Parsing was unsuccessful, likely because of broken argument.");
  }
  PL_RETURN_IF_ERROR(result);
  std::cout << ir->dag().DebugString() << std::endl;
  std::cout << ir->Get(0)->DebugString(0) << std::endl;
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
StatusOr<std::shared_ptr<IR>> parseQuery(const std::string& query_str) {
  // create file that contains queryString
  std::string filename = setupQuery(query_str);
  // parse to AST and walk
  auto status = parseFile(filename);
  // clean up
  std::remove(filename.c_str());
  // TODO(philkuz) figure out how to change log verbosity.
  std::cout << status.status().ToString() << std::endl;
  return status;
}

// Checks whether we can actually compile into a graph.
TEST(ASTVisitor, compilation_test) {
  std::string from_expr = R"(
From(table="cpu", select=["cpu0", "cpu1"])
)";
  auto ig_status = parseQuery(from_expr);
  EXPECT_OK(ig_status);
  auto ig = ig_status.ValueOrDie();
  VerifyGraphConnections(ig.get());
  // check the connection of ig
  std::string from_range_expr = R"(
From(table="cpu", select=["cpu0"]).Range(time="-2m");
)";
  EXPECT_OK(parseQuery(from_range_expr));
}

// Checks whether the IR graph constructor can identify bads args.
TEST(ASTVisitor, bad_arguments) {
  std::string extra_from_args = R"(
From(table="cpu", select=["cpu0"], fakeArg="hahaha").Range(time="-2m");
)";
  EXPECT_FALSE(parseQuery(extra_from_args).ok());
  std::string missing_from_args = R"(
From(table="cpu").Range(time="-2m");
)";
  EXPECT_FALSE(parseQuery(missing_from_args).ok());
  std::string no_from_args = R"(
From().Range(time="-2m");
)";
  EXPECT_FALSE(parseQuery(no_from_args).ok());
}
// Checks to make sure the parser identifies bad syntax
TEST(ASTVisitor, bad_syntax) {
  std::string early_paranetheses_close = R"(
From);
)";
  EXPECT_FALSE(parseQuery(early_paranetheses_close).ok());
}
// Checks to make sure the compiler can catch operators that don't exist.
TEST(ASTVisitor, nonexistant_operator_names) {
  std::string wrong_from_op_name = R"(
Drom(table="cpu", select=["cpu0"]).Range(time="-2m");
)";
  EXPECT_FALSE(parseQuery(wrong_from_op_name).ok());
  std::string wrong_range_op_name = R"(
From(table="cpu", select=["cpu0"]).BRange(time="-2m");
)";
  EXPECT_FALSE(parseQuery(wrong_range_op_name).ok());
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
