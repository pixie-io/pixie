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

genrule(
    name = "mysql_parser_gen",
    srcs = glob(["sql/mysql/Positive-Technologies/*.g4"]),
    outs = [
        "mysql_parser/MySQLLexer.cpp",
        "mysql_parser/MySQLLexer.h",
        "mysql_parser/MySQLParser.cpp",
        "mysql_parser/MySQLParser.h",
        "mysql_parser/MySQLParserBaseListener.cpp",
        "mysql_parser/MySQLParserBaseListener.h",
        "mysql_parser/MySQLParserListener.cpp",
        "mysql_parser/MySQLParserListener.h",
    ],
    cmd = """
  OUT_DIR=`dirname $(location mysql_parser/MySQLParser.h)`;
  OUT_DIR_ABS_PATH=`realpath $$OUT_DIR`;
  GRAMMAR_DIR=`dirname $(location sql/mysql/Positive-Technologies/MySQLParser.g4)`;
  cd $$GRAMMAR_DIR;
  java -Xmx500m -jar /opt/antlr/antlr-4.9-complete.jar -Dlanguage=Cpp -o $$OUT_DIR_ABS_PATH -package mysql_parser -listener MySQLLexer.g4 MySQLParser.g4;
  """,
)

cc_library(
    name = "libmysql_parser",
    srcs = [":mysql_parser_gen"],
    hdrs = [
        "mysql_parser/MySQLLexer.h",
        "mysql_parser/MySQLParser.h",
        "mysql_parser/MySQLParserBaseListener.h",
        "mysql_parser/MySQLParserListener.h",
    ],
    deps = [
        "@com_github_antlr_antlr4//:libantlr",
    ],
    visibility = ["//visibility:public"],
)
