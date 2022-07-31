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
    name = "bcc_source",
    srcs = glob(["**"]),
)

# This rule is a hack so that local development of bcc can be done without bazel rerunning the full
# bcc build each time. It assumes that the following commands have been run in the local bcc
# directory:
#
#   mkdir -p build && cd build
#   cmake -DCMAKE_INSTALL_PREFIX=install \
#       -DENABLE_EXAMPLES=OFF -DENABLE_MAN=OFF -DENABLE_TESTS=OFF ..
#   make install
#
# Then anytime you update bcc sources, you have to run `make install` in the bcc build dir, and then
# run bazel build.
#
# Note#1: Since bpftrace uses bcc as a dependency, if you want to get the benefits of the local
# incremental builds for bpftrace, you have to build both bcc and bpftrace locally.
#
# Note#2: The cmake definitions above should be consistent with `cache-entires` of
# bazel/external/bcc.BUILD.
local_cc(
    name = "bcc",
    install_prefix = "build/install",
    lib_source = ":bcc_source",
    linkopts = [
        # ELF binary parsing.
        "-lelf",
    ],
    out_include_dir = "include",
    out_lib_dir = "lib",
    out_static_libs = [
        "libapi-static.a",
        "libbcc.a",
        "libbcc_bpf.a",
        "libbcc-loader-static.a",
        "libclang_frontend.a",
    ],
    visibility = ["//visibility:public"],
)
