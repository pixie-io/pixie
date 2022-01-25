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

# In order to get the same build as the production bpftrace build, you should run the following commands in the bpftrace local repo:
#   mkdir build && cd build
#   cmake -DCMAKE_INSTALL_PREFIX=install -DBUILD_TESTING=OFF -DENABLE_BFD_DISABLE=OFF -DENABLE_LIBDW=OFF -DENABLE_MAN=OFF
#       -DLIBBCC_BPF_LIBRARIES=$BCC_INSTALL/lib/libbcc_bpf.a -DLIBBCC_INCLUDE_DIRS=$BCC_INSTALL/include
#       -DLIBBCC_LIBRARIES=$BCC_INSTALL/lib/libbcc.a
#       -DLIBBCC_LOADER_LIBRARY_STATIC=$BCC_INSTALL/lib/libbcc-loader-static.a -DCMAKE_BUILD_TYPE=Release ..
#   make install
# Everytime you make a local change to bcc and/or bpftrace you have to run make install in the local bcc repo and
# then the local bpftrace repo.
# Note: you may need to install the ubuntu package `libcereal-dev` to get it to work.
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
    ],
)
