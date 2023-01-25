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

#pragma once
#include <antlr4-runtime.h>
#include <atn/ParserATNSimulator.h>
#include <atn/PredictionMode.h>
#include <simdutf.h>

#include <absl/strings/substitute.h>
#include <cctype>
#include <memory>
#include <string>
#include <vector>
#include "src/common/base/error.h"
#include "src/common/base/logging.h"
#include "src/common/base/status.h"
#include "src/common/base/statusor.h"

namespace px {
namespace carnot {
namespace builtins {
namespace sql_parsing {

/**
 * UpperCaseCharStream makes the parser view the input stream as entirely upper case.
 * This is required for the MySQL parser, since the grammar is specified only as upper case.
 */
class UpperCaseCharStream : public antlr4::ANTLRInputStream {
 public:
  explicit UpperCaseCharStream(std::string input) : antlr4::ANTLRInputStream(input) {}

  size_t LA(ssize_t i) override {
    auto input = antlr4::ANTLRInputStream::LA(i);
    return toupper(input);
  }
};

/**
 * StatusErrorListener listens for errors during the Antlr parsing process.
 * After parsing, the caller is expected to call GetStatus() to check for parsing errors.
 */
class StatusErrorListener : public antlr4::BaseErrorListener {
 public:
  void syntaxError(antlr4::Recognizer*, antlr4::Token*, size_t line, size_t charPositionInLine,
                   const std::string& msg, std::exception_ptr) override {
    errors.push_back(absl::Substitute("line $0:$1 with: $2", line, charPositionInLine, msg));
  }
  Status GetStatus() {
    if (errors.empty()) {
      return Status::OK();
    }
    return error::InvalidArgument("SQL Parsing Failed: $0", absl::StrJoin(errors, "\n"));
  }

  void Reset() { errors.clear(); }

 private:
  std::vector<std::string> errors;
};

template <typename TParser, typename TLexer, typename TCharStream = antlr4::ANTLRInputStream>
class AntlrParser {
 public:
  explicit AntlrParser(std::string input, bool sll_only = false)
      : input_(input), sll_only_(sll_only) {}

  Status ParseWalk(antlr4::tree::ParseTreeListener* listener) {
    // Because of the way Antlr handles memory it makes it awkward to do the parsing and then later
    // walk through the parse tree. So instead we parse and walk the tree in the same function.

    if (!simdutf::validate_utf8(input_.data(), input_.length())) {
      return error::InvalidArgument("Invalid UTF-8 byte sequence in SQL query");
    }
    TCharStream input_stream(input_);
    TLexer lexer(&input_stream);
    antlr4::CommonTokenStream tokens(&lexer);
    TParser parser(&tokens);
    parser.removeErrorListener(&antlr4::ConsoleErrorListener::INSTANCE);
    StatusErrorListener error_listener;
    parser.addErrorListener(&error_listener);
    parser.template getInterpreter<antlr4::atn::ParserATNSimulator>()->setPredictionMode(
        antlr4::atn::PredictionMode::SLL);

    antlr4::tree::ParseTree* parse_tree;
    tokens.fill();
    parse_tree = parser.root();

    auto status = error_listener.GetStatus();
    // Two-stage parsing strategy. First try with fast SLL mode. If that fails then do the LL* mode.
    // Ideally, all queries should be able to be handled by SLL mode, but in case some queries
    // slip through the cracks we have LL* mode as a backup. If sll_only_ is set, then we fail if
    // SLL mode fails.
    if (sll_only_ && !status.ok()) {
      return status;
    } else if (!status.ok()) {
      error_listener.Reset();
      parser.reset();
      parser.removeErrorListener(&antlr4::ConsoleErrorListener::INSTANCE);
      parser.addErrorListener(&error_listener);
      parser.template getInterpreter<antlr4::atn::ParserATNSimulator>()->setPredictionMode(
          antlr4::atn::PredictionMode::LL);

      parse_tree = parser.root();
      PX_RETURN_IF_ERROR(error_listener.GetStatus());
    }

    antlr4::tree::ParseTreeWalker walker;
    walker.walk(listener, parse_tree);
    return Status::OK();
  }

 private:
  std::string input_;
  bool sll_only_;
};

}  // namespace sql_parsing
}  // namespace builtins
}  // namespace carnot
}  // namespace px
