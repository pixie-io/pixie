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

filegroup(
    name = "toolchain_compiler_files",
    srcs = [
        ":as",
        ":clang",
        ":toolchain_include",
    ],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "toolchain_linker_files",
    srcs = [
        ":ar",
        ":clang",
        ":lld",
        ":toolchain_lib",
    ],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "toolchain_ar_files",
    srcs = [":ar"],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "toolchain_as_files",
    srcs = [":as"],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "toolchain_dwp_files",
    srcs = [":dwp"],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "toolchain_objcopy_files",
    srcs = [":objcopy"],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "toolchain_strip_files",
    srcs = [":strip"],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "toolchain_include",
    srcs = glob([
        "{toolchain_path}/lib/clang/{clang_version}/include/**",
    ]),
)

filegroup(
    name = "toolchain_lib",
    srcs = glob([
        "{toolchain_path}/lib/libc++*",
        "{toolchain_path}/lib/clang/{clang_version}/lib/linux/**",
    ]),
)

filegroup(
    name = "clang",
    srcs = [
        "{toolchain_path}/bin/clang",
        "{toolchain_path}/bin/clang++",
        "{toolchain_path}/bin/clang-cpp",
        "{toolchain_path}/bin/clang-{clang_major_version}",
    ],
)

filegroup(
    name = "lld",
    srcs = [
        "{toolchain_path}/bin/ld.lld",
        "{toolchain_path}/bin/lld",
    ],
)

[
    filegroup(
        name = binary,
        srcs = [
            "{toolchain_path}/bin/llvm-" + binary,
        ],
    )
    for binary in [
        "ar",
        "as",
        "cov",
        "objcopy",
        "strip",
        "dwp",
    ]
]

filegroup(
    name = "toolchain_all_files",
    srcs = [
        ":toolchain_ar_files",
        ":toolchain_as_files",
        ":toolchain_compiler_files",
        ":toolchain_dwp_files",
        ":toolchain_linker_files",
        ":toolchain_objcopy_files",
        ":toolchain_strip_files",
    ],
    visibility = ["//visibility:public"],
)
