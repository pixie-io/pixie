#include "src/carnot/compiler/ast_visitor.h"
#include <glog/logging.h>
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
    tmpFileName = absl::StrFormat("%s/%s.pl", tmpDir, genRandomStr(10));
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
  LOG(INFO) << ir->dag().DebugString() << std::endl;
  LOG(INFO) << ir->DebugString() << std::endl;
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
  LOG(INFO) << status.status().ToString() << std::endl;
  return status;
}

// Checks whether we can actually compile into a graph.
TEST(ASTVisitor, compilation_test) {
  std::string from_expr = R"(
From(table='cpu', select=['cpu0', 'cpu1'])
)";
  auto ig_status = parseQuery(from_expr);
  EXPECT_OK(ig_status);
  auto ig = ig_status.ValueOrDie();
  VerifyGraphConnections(ig.get());
  // check the connection of ig
  std::string from_range_expr = R"(
From(table='cpu', select=['cpu0']).Range(time='-2m');
)";
  EXPECT_OK(parseQuery(from_range_expr));
}

// Checks whether the IR graph constructor can identify bads args.
TEST(ASTVisitor, bad_arguments) {
  std::string extra_from_args = R"(
From(table='cpu', select=['cpu0'], fakeArg='hahaha').Range(time='-2m');
)";
  EXPECT_FALSE(parseQuery(extra_from_args).ok());
  std::string missing_from_args = R"(
From(table='cpu').Range(time='-2m');
)";
  EXPECT_FALSE(parseQuery(missing_from_args).ok());
  std::string no_from_args = R"(
From().Range(time='-2m');
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
Drom(table='cpu', select=['cpu0']).Range(time='-2m');
)";
  EXPECT_FALSE(parseQuery(wrong_from_op_name).ok());
  std::string wrong_range_op_name = R"(
From(table='cpu', select=['cpu0']).BRange(time='-2m');
)";
  EXPECT_FALSE(parseQuery(wrong_range_op_name).ok());
}
TEST(ASTVisitor, assign_functionality) {
  std::string simple_assign = R"(
queryDF = From(table='cpu', select=['cpu0', 'cpu1'])
)";
  EXPECT_OK(parseQuery(simple_assign));
  std::string assign_and_use = R"(
queryDF = From(table='cpu', select=['cpu0', 'cpu1'])
queryDF.Range(time='-2m')
)";
  EXPECT_OK(parseQuery(assign_and_use));
}
TEST(ASTVisitor, assign_error_checking) {
  std::string bad_assign_mult_values = R"(
queryDF,haha = From(table='cpu', select=['cpu0', 'cpu1'])
queryDF.Range(time='-2m')
)";
  EXPECT_FALSE(parseQuery(bad_assign_mult_values).ok());
  std::string bad_assign_str = R"(
queryDF = 'str'
)";
  EXPECT_FALSE(parseQuery(bad_assign_str).ok());
}
// Map Tests
TEST(MapTest, single_col_map) {
  std::string single_col_map_sum = R"(
queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')
rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1})
)";
  EXPECT_OK(parseQuery(single_col_map_sum));
  std::string single_col_div_map_query = R"(
queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')
rangeDF = queryDF.Map(fn=lambda r : {'sum' : pl.div(r.cpu0,r.cpu1)})
)";
  EXPECT_OK(parseQuery(single_col_div_map_query));
}

TEST(MapTest, multi_col_map) {
  std::string multi_col = R"(
queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')
rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1, 'copy' : r.cpu2})
)";
  EXPECT_OK(parseQuery(multi_col));
}

TEST(MapTest, bin_op_test) {
  std::string single_col_map_sum = R"(
queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')
rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1})
)";
  EXPECT_OK(parseQuery(single_col_map_sum));
  std::string single_col_map_sub = R"(
queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')
rangeDF = queryDF.Map(fn=lambda r : {'sub' : r.cpu0 - r.cpu1})
)";
  EXPECT_OK(parseQuery(single_col_map_sub));
  std::string single_col_map_product = R"(
queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')
rangeDF = queryDF.Map(fn=lambda r : {'product' : r.cpu0 * r.cpu1})
)";
  EXPECT_OK(parseQuery(single_col_map_product));
  std::string single_col_map_quotient = R"(
queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')
rangeDF = queryDF.Map(fn=lambda r : {'quotient' : r.cpu0 / r.cpu1})
)";
  EXPECT_OK(parseQuery(single_col_map_quotient));
}

TEST(MapTest, nested_expr_map) {
  std::string nested_expr = R"(
queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')
rangeDF = queryDF.Map(fn=lambda r : {'sum' : r.cpu0 + r.cpu1 + r.cpu2})
)";
  EXPECT_OK(parseQuery(nested_expr));
  std::string nested_fn = R"(
queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')
rangeDF = queryDF.Map(fn=lambda r : {'sum' : pl.div(r.cpu0 + r.cpu1, r.cpu2)})
)";
  EXPECT_OK(parseQuery(nested_fn));
}

TEST(AggTest, single_col_agg) {
  std::string single_col_agg = R"(
queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')
rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_count' : pl.count(r.cpu1)})
)";
  EXPECT_OK(parseQuery(single_col_agg));
  std::string multi_output_col_agg = R"(
queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')
rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {
  'cpu_count' : pl.count(r.cpu1),
  'cpu_mean' : pl.mean(r.cpu1)})
)";
  EXPECT_OK(parseQuery(multi_output_col_agg));
  std::string multi_input_col_agg = R"(
queryDF = From(table='cpu', select=['cpu0', 'cpu1', 'cpu2']).Range(time='-2m')
rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=lambda r : {'cpu_sum' : pl.sum(r.cpu1), 'cpu2_mean' : pl.mean(r.cpu2)})
)";
  EXPECT_OK(parseQuery(multi_input_col_agg));
}
TEST(AggTest, not_allowed_by) {
  std::string single_col_bad_by_fn = R"(
queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')
rangeDF = queryDF.Agg(by=lambda r : r, fn=lambda r :  {'cpu_count' : pl.count(r.cpu0)})
)";
  EXPECT_FALSE(parseQuery(single_col_bad_by_fn).ok());
  // TODO(philkuz) punt to verification test.
  //   std::string single_col_bad_by_fn_expr = R"(
  // queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')
  // rangeDF = queryDF.Agg(by=lambda r : 1+2, fn=pl.count)
  // )";
  //   EXPECT_FALSE(parseQuery(single_col_bad_by_fn_expr).ok());

  std::string single_col_bad_by_attr = R"(
  queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')
  rangeDF = queryDF.Agg(by=pl.mean, fn={'cpu_count' : pl.count(r.cpu0)})
  )";
  EXPECT_FALSE(parseQuery(single_col_bad_by_attr).ok());

  // TODO(philkuz) punt to verification test.
  //   std::string single_col_dict_by_fn = R"(
  // queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')
  // rangeDF = queryDF.Agg(by=lambda r : {'cpu' : r.cpu0}, fn={'cpu_count' : pl.count(r.cpu0)})
  // )";
  //   EXPECT_FALSE(parseQuery(single_col_dict_by_fn).ok());
}
TEST(AggTest, not_allowed_agg_fn) {
  std::string single_col_bad_agg_fn = R"(
queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')
rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=1+2)
)";
  EXPECT_FALSE(parseQuery(single_col_bad_agg_fn).ok());
  std::string single_col_dict_by_not_pl = R"(
queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')
rangeDF = queryDF.Agg(by=lambda r : r.cpu0, fn=notpl.count)
)";
  EXPECT_FALSE(parseQuery(single_col_dict_by_not_pl).ok());
  std::string single_col_dict_by_no_attr_fn = R"(
queryDF = From(table='cpu', select=['cpu0', 'cpu1']).Range(time='-2m')
rangeDF = queryDF.Agg(by=lambda r :r.cpu0, fn=count)
)";
  EXPECT_FALSE(parseQuery(single_col_dict_by_no_attr_fn).ok());
  std::string valid_fn_not_valid_call = absl::StrJoin(
      {"queryDF = From(table = 'cpu', select = [ 'cpu0', 'cpu1' ]).Range(time = '-2m')",
       "rangeDF =queryDF.Agg(by = lambda r: r.cpu0, fn = pl.count) "},
      "\n");
  EXPECT_FALSE(parseQuery(valid_fn_not_valid_call).ok());
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
