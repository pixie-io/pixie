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

load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")
load("@rules_java//java:defs.bzl", "java_binary")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

cmake(
    name = "libantlr",
    build_args = [
        "--",  # <- Pass remaining options to the native tool.
        "-j`nproc`",
        "-l`nproc`",
    ],
    cache_entries = {
        "ANTLR_BUILD_CPP_TESTS": "OFF",
    },
    lib_source = ":all",
    out_include_dir = "include/antlr4-runtime",
    out_static_libs = [
        "libantlr4-runtime.a",
    ],
    visibility = ["//visibility:public"],
    working_directory = "runtime/Cpp",
)

java_binary(
    name = "antlr",
    jvm_flags = ["-Xmx500m"],
    main_class = "org.antlr.v4.Tool",
    visibility = ["//visibility:public"],
    runtime_deps = [
        "@px_deps//:org_antlr_antlr4",
    ],
)
