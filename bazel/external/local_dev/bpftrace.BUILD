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

load("@//bazel:local_cc.bzl", "local_cc")

licenses(["notice"])

filegroup(
    name = "bpftrace_source",
    srcs = glob(["**"]),
)

# In order to use this rule, you should first follow the setup in local_dev/bcc.BUILD.
# Then you should run the following in the pixie repo:
#   export LIBCEREAL_INCLUDE_DIR=`bazel info output_base`/external/com_github_USCiLab_cereal/include
# Then you should run the following at the top of the bcc repo:
#   export BCC_INSTALL=`pwd`/build/install
# Then run the following in the bpftrace repo:
#
#   mkdir -p build && cd build
#   cmake \
#       -DCMAKE_INSTALL_PREFIX=install \
#       -DBUILD_FUZZ=OFF \
#       -DBUILD_TESTING=OFF \
#       -DENABLE_BFD_DISABLE=OFF \
#       -DENABLE_BPFTRACE_EXE=OFF \
#       -DENABLE_LIBDW=OFF \
#       -DENABLE_MAN=OFF \
#       -DENABLE_SKB_OUTPUT=OFF \
#       -DLIBBCC_BPF_LIBRARIES=$BCC_INSTALL/lib/libbcc_bpf.a \
#       -DLIBBCC_INCLUDE_DIRS=$BCC_INSTALL/include \
#       -DLIBBCC_LIBRARIES=$BCC_INSTALL/lib/libbcc.a \
#       -DLIBBCC_LOADER_LIBRARY_STATIC=$BCC_INSTALL/lib/libbcc-loader-static.a \
#       -DLIBBPF_INCLUDE_DIRS=$LIBBPF_PREFIX/libbpf/include \
#       -DLIBBPF_LIBRARIES=$LIBBPF_PREFIX/libbpf/lib64/libbpf.a \
#       -DLIBCEREAL_INCLUDE_DIRS=$LIBCEREAL_INCLUDE_DIR \
#       -DCMAKE_BUILD_TYPE=Release ..
#   make -j$(nproc) install
#
# Every time you make a local change to bcc and/or bpftrace you have to run make install
# in the local repos.
#
# Note#1: The cmake definitions above should be consistent with `cache-entires` of
# bazel/external/bpftrace.BUILD.
local_cc(
    name = "bpftrace",
    install_prefix = "build/install",
    lib_source = ":bpftrace_source",
    linkopts = [
        "-lelf",
        "-lz",
    ],
    out_include_dir = "include",
    out_lib_dir = "lib",
    out_static_libs = [
        "libbpftrace.a",
        "libaot.a",
        "libast.a",
        "libruntime.a",
        "libbpforc.a",
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
    ],
)
