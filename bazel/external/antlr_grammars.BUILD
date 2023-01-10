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
    name = "mysql_parser_gen",
    srcs = [
        "sql/mysql/Positive-Technologies/MySQLLexer.g4",
        "sql/mysql/Positive-Technologies/MySQLParser.g4",
    ],
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
        OUT=`dirname $(location mysql_parser/MySQLLexer.cpp)`

        $(location @com_github_antlr_antlr4//:antlr) \
            -Dlanguage=Cpp \
            -o $$OUT \
            -package mysql_parser \
            -listener \
            -Xexact-output-dir \
            $(location sql/mysql/Positive-Technologies/MySQLLexer.g4) \
            $(location sql/mysql/Positive-Technologies/MySQLParser.g4)
    """,
    tools = ["@com_github_antlr_antlr4//:antlr"],
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
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_antlr_antlr4//:libantlr",
    ],
)
