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

licenses(["notice"])

filegroup(
    name = "bcc_source",
    srcs = glob(["**/*"]),
)

cmake(
    name = "bcc",
    generate_crosstool_file = True,
    lib_name = "libbcc_static",
    lib_source = ":bcc_source",
    # These link opts are dependencies of bcc.
    linkopts = [
        # ELF binary parsing.
        "-lelf",
    ],
    make_commands = [
        "make -j$(nproc) -C src/cc install",
    ],
    out_static_libs = [
        "libapi-static.a",
        "libbcc.a",
        "libbcc_bpf.a",
        "libbcc-loader-static.a",
        "libb_frontend.a",
        "libclang_frontend.a",
        "libusdt-static.a",
    ],
    visibility = ["//visibility:public"],
)
