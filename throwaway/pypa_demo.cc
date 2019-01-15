#include <glog/logging.h>

#include <pypa/parser/parser.hh>

namespace pypa {
void dump(AstPtr);
}

int main(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);

  pypa::AstModulePtr ast;
  pypa::SymbolTablePtr symbols;
  pypa::ParserOptions options;

  options.printerrors = true;
  options.printdbgerrors = true;
  if (argc != 2) {
    LOG(FATAL) << "USAGE: pypa_demo <python_file>";
  }
  pypa::Lexer lexer(argv[1]);
  if (pypa::parse(lexer, ast, symbols, options)) {
    LOG(INFO) << "Parsing successfull";
    pypa::dump(ast);
    return 0;
  }
  LOG(ERROR) << "Failed to parse.";
}
