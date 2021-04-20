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

licenses(["notice"])

exports_files([
    "COPYING",
])

cc_library(
    name = "libpypa",
    srcs = [
        "src/pypa/ast/ast.cc",
        "src/pypa/ast/dump.cc",
        "src/pypa/filebuf.cc",
        "src/pypa/lexer/lexer.cc",
        "src/pypa/parser/make_string.cc",
        "src/pypa/parser/parser.cc",
        "src/pypa/parser/symbol_table.cc",
    ],
    hdrs = glob([
        "src/**/*.hh",
        "src/**/*.inl",
    ]),
    copts = [
        "-Wno-unused-local-typedef",
        "-Wno-unused-parameter",
    ],
    includes = ["src"],
    linkopts = [
        "-lm",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_double_conversion//:double-conversion",
    ],
)
