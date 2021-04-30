# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:defs.bzl", "cc_library")

genrule(
    name = "pgsql_parser_gen",
    srcs = [
        "apgdiff/antlr-src/PostgresSQLLexer.g4",
        "apgdiff/antlr-src/PostgresSQLParser.g4",
    ],
    outs = [
        "pgsql_parser/PostgresSQLLexer.cpp",
        "pgsql_parser/PostgresSQLLexer.h",
        "pgsql_parser/PostgresSQLParser.cpp",
        "pgsql_parser/PostgresSQLParser.h",
        "pgsql_parser/PostgresSQLParserBaseListener.cpp",
        "pgsql_parser/PostgresSQLParserBaseListener.h",
        "pgsql_parser/PostgresSQLParserListener.cpp",
        "pgsql_parser/PostgresSQLParserListener.h",
    ],
    cmd = """
  OUT_DIR=`dirname $(location pgsql_parser/PostgresSQLParser.h)`;
  OUT_DIR_ABS_PATH=`realpath $$OUT_DIR`;
  GRAMMAR_DIR=`dirname $(location apgdiff/antlr-src/PostgresSQLParser.g4)`;
  cd $$GRAMMAR_DIR;
  java -Xmx500m -jar /opt/antlr/antlr-4.9-complete.jar -Dlanguage=Cpp -o $$OUT_DIR_ABS_PATH -package pgsql_parser -listener PostgresSQLLexer.g4 PostgresSQLParser.g4;
  """,
)

cc_library(
    name = "libpgsql_parser",
    srcs = [":pgsql_parser_gen"],
    hdrs = [
        "pgsql_parser/PostgresSQLLexer.h",
        "pgsql_parser/PostgresSQLParser.h",
        "pgsql_parser/PostgresSQLParserBaseListener.h",
        "pgsql_parser/PostgresSQLParserListener.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_antlr_antlr4//:libantlr",
        "@com_google_absl//absl/strings",
    ],
)
