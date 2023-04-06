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

load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

filegroup(
    name = "all",
    srcs = glob(["**"]),
)

configure_make(
    name = "musl",
    configure_options = ["--disable-shared"] + select({
        "@platforms//cpu:aarch64": [
            "--target",
            "aarch64",
        ],
        "//conditions:default": [],
    }),
    lib_source = ":all",
    out_bin_dir = "lib",
    out_binaries = [
        "Scrt1.o",
        "crt1.o",
        "crti.o",
        "crtn.o",
        "rcrt1.o",
    ],
    out_static_libs = ["libc.a"],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "Scrt1.o",
    srcs = [":musl"],
    output_group = "Scrt1.o",
    visibility = ["//visibility:public"],
)

filegroup(
    name = "crt1.o",
    srcs = [":musl"],
    output_group = "crt1.o",
    visibility = ["//visibility:public"],
)

filegroup(
    name = "crti.o",
    srcs = [":musl"],
    output_group = "crti.o",
    visibility = ["//visibility:public"],
)

filegroup(
    name = "crtn.o",
    srcs = [":musl"],
    output_group = "crtn.o",
    visibility = ["//visibility:public"],
)

filegroup(
    name = "rcrt1.o",
    srcs = [":musl"],
    output_group = "rcrt1.o",
    visibility = ["//visibility:public"],
)
