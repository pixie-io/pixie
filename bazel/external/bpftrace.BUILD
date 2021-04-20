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

load("@px//bazel:flex_bison.bzl", "genbison", "genflex")
load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])

# When building bpftrace, it automatically generates the following file:
# <build_dir>/resources/headers.cpp
# Due to build issues, this file was copied directly into src, and checked in to our fork of bpftrace.
# If at any time, bpftrace is updated from the origin, AND there are changes to the resources directory,
# then the updated headers.cpp should be copied and checked in to src.
# If there are no changes to files in the resources directory (which is the likely case), then no special steps are required.
cc_library(
    name = "bpftrace",
    srcs = glob(
        ["src/*.cpp"],
        exclude = [
            "src/main.cpp",
            "src/bfd-disasm.cpp",
        ],
    ) + glob(
        [
            "src/*.h",
            "src/arch/*.h",
            "src/ast/*.h",
        ],
    ),
    hdrs = glob([
        "src/*.h",
        "src/arch/*.h",
        "src/ast/*.h",
    ]),
    copts = [
        "-w",
    ],
    defines = [
        "HAVE_NAME_TO_HANDLE_AT=1",
        "HAVE_BCC_PROG_LOAD=1",
        "HAVE_BCC_CREATE_MAP=1",
        "HAVE_BCC_ELF_FOREACH_SYM=1",
        "HAVE_BCC_KFUNC=1",
        "HAVE_BCC_USDT_ADDSEM=1",
        "LIBBCC_ATTACH_KPROBE_SIX_ARGS_SIGNATURE=1",
    ],
    include_prefix = "bpftrace",
    tags = ["linux_only"],
    visibility = ["//visibility:public"],
    deps = [
        ":bpftrace-arch",
        ":bpftrace-ast",
    ],
)

cc_library(
    name = "bpftrace-arch",
    srcs = [
        "src/arch/x86_64.cpp",
    ],
    hdrs = glob([
        "src/arch/*.h",
    ]),
    copts = [
        "-w",
    ],
    visibility = ["//visibility:private"],
)

cc_library(
    name = "bpftrace-ast",
    srcs = glob([
        "src/ast/*.cpp",
    ]),
    hdrs = glob([
        "src/ast/*.h",
        "src/arch/*.h",
    ]),
    copts = [
        "-w",
    ],
    tags = ["linux_only"],
    visibility = ["//visibility:private"],
    deps = [
        ":bpftrace-parser",
        "@com_github_iovisor_bcc//:bcc",
        "@com_llvm_lib//:llvm",
    ],
)

cc_library(
    name = "bpftrace-parser",
    srcs = [
        ":bpftrace-bison",
        ":bpftrace-flex",
    ],
    hdrs = glob([
        "src/*.h",
        "src/libbpf/*.h",
        "src/ast/*.h",
    ]) + [
        ":bpftrace-bison",
    ],
    copts = [
        "-w",
    ],
    includes = [
        "src",
        "src/ast",
    ],
    tags = ["linux_only"],
    deps = [
        "@com_github_iovisor_bcc//:bcc",
    ],
)

genbison(
    name = "bpftrace-bison",
    src = "src/parser.yy",
    extra_outs = [
        "stack.hh",
        "location.hh",
        "position.hh",
    ],
    header_out = "parser.tab.hh",
    source_out = "parser.tab.cc",
    tags = ["linux_only"],
)

genflex(
    name = "bpftrace-flex",
    src = "src/lexer.l",
    out = "lex.yy.cc",
    includes = [":bpftrace-bison"],
    tags = ["linux_only"],
)
