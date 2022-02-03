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

exports_files(["LICENSE"])

cc_library(
    name = "double-conversion",
    srcs = [
        "double-conversion/bignum.cc",
        "double-conversion/bignum-dtoa.cc",
        "double-conversion/cached-powers.cc",
        "double-conversion/double-to-string.cc",
        "double-conversion/fast-dtoa.cc",
        "double-conversion/fixed-dtoa.cc",
        "double-conversion/string-to-double.cc",
        "double-conversion/strtod.cc",
    ],
    hdrs = [
        "double-conversion/bignum.h",
        "double-conversion/bignum-dtoa.h",
        "double-conversion/cached-powers.h",
        "double-conversion/diy-fp.h",
        "double-conversion/double-conversion.h",
        "double-conversion/double-to-string.h",
        "double-conversion/fast-dtoa.h",
        "double-conversion/fixed-dtoa.h",
        "double-conversion/ieee.h",
        "double-conversion/string-to-double.h",
        "double-conversion/strtod.h",
        "double-conversion/utils.h",
    ],
    includes = ["."],
    linkopts = [
        "-lm",
    ],
    visibility = ["//visibility:public"],
)
