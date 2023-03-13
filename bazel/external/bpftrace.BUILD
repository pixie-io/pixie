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
    name = "bpftrace_source",
    srcs = glob(["**"]),
)

cmake(
    name = "bpftrace",
    build_args = [
        "--",  # <- Pass remaining options to the native tool.
        "-j`nproc`",
        "-l`nproc`",
    ],
    build_data = llvm_build_data_deps(),
    cache_entries = add_llvm_cache_entries({
        "BUILD_FUZZ": "OFF",
        "BUILD_TESTING": "OFF",

        # Disable the use of certain libraries, even if found on the system.
        # This effectively disables certain features, but we don't currently rely on those features,
        # so we want to keep things slimmer.
        "ENABLE_BFD_DISASM": "OFF",
        "ENABLE_BPFTRACE_EXE": "OFF",
        "ENABLE_LIBDW": "OFF",
        "ENABLE_MAN": "OFF",
        "ENABLE_SKB_OUTPUT": "OFF",

        # Provide paths to dependent binaries: libbpf, bcc and libcereal.
        # Notice that bcc and libceral are in the bazel deps below as well.
        # $EXT_BUILD_DEPS is a macro that points to where dependencies are built.
        "LIBBCC_BPF_LIBRARIES": "$EXT_BUILD_DEPS/bcc/lib/libbcc_bpf.a",
        "LIBBCC_INCLUDE_DIRS": "$EXT_BUILD_DEPS/bcc/include",
        "LIBBCC_LIBRARIES": "$EXT_BUILD_DEPS/bcc/lib/libbcc.a",
        "LIBBCC_LOADER_LIBRARY_STATIC": "$EXT_BUILD_DEPS/bcc/lib/libbcc-loader-static.a",
        "LIBBPF_INCLUDE_DIRS": "$EXT_BUILD_DEPS/libbpf/include",
        "LIBBPF_LIBRARIES": "$EXT_BUILD_DEPS/libbpf/lib64/libbpf.a",
        "LIBCEREAL_INCLUDE_DIRS": "$EXT_BUILD_DEPS/include",
    }),
    lib_source = ":bpftrace_source",
    linkopts = [
        "-lelf",
        "-lz",
    ],
    out_static_libs = [
        "libbpftrace.a",
        "libaot.a",
        "libast.a",
        "libruntime.a",
        "libast_defs.a",
        "libparser.a",
        "libresources.a",
        "libarch.a",
        "libcxxdemangler_stdlib.a",
        "libcxxdemangler_llvm.a",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_USCiLab_cereal//:cereal",
        "@com_github_iovisor_bcc//:bcc",
        "@com_github_libbpf_libbpf//:libbpf",
        "@px//:llvm",
    ],
)
