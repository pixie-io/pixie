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

load("@px//bazel:llvm_cmake.bzl", "add_llvm_cache_entries", "llvm_build_data_deps")
load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

licenses(["notice"])

filegroup(
    name = "bcc_source",
    srcs = glob(["**"]),
)

cmake(
    name = "bcc",
    build_args = [
        "--",  # <- Pass remaining options to the native tool.
        "-j`nproc`",
        "-l`nproc`",
    ],
    build_data = llvm_build_data_deps(),
    cache_entries = add_llvm_cache_entries({
        "CMAKE_USE_LIBBPF_PACKAGE": "ON",
        "ENABLE_EXAMPLES": "OFF",
        "ENABLE_MAN": "OFF",
        "ENABLE_TESTS": "OFF",
        "LIBBPF_INCLUDE_DIR": "$EXT_BUILD_DEPS/libbpf/include",
        "LIBBPF_LIBRARIES": "$EXT_BUILD_DEPS/libbpf/lib64/libbpf.a",
    }),
    includes = [
        "bcc/compat",
    ],
    install = False,
    lib_source = ":bcc_source",
    # These link opts are dependencies of bcc.
    linkopts = [
        # ELF binary parsing.
        "-lelf",
    ],
    out_static_libs = [
        "libapi-static.a",
        "libbcc.a",
        "libbcc_bpf.a",
        "libbcc-loader-static.a",
        "libclang_frontend.a",
    ],
    postfix_script = "make -C src/cc install",
    targets = [
        "api-static",
        "bcc-static",
        "bcc-loader-static",
        "bpf-static",
        "clang_frontend",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_libbpf_libbpf//:libbpf",
        "@px//:llvm",
    ],
)
