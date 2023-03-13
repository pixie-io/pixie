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

load("@px//bazel/cc_toolchains/sysroots:sysroots.bzl", "sysroot_toolchain")

filegroup(
    name = "all_files",
    srcs = [
        ":compiler_files",
        ":linker_files",
    ],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "compiler_files",
    srcs = glob([
        "usr/local/include/**",
        "usr/include/{target_arch}-linux-{abi}/**",
        "usr/include/**",
        "usr/include/c++/12/**",
        "usr/include/{target_arch}-linux-{abi}/c++/12/**",
        "usr/include/c++/12/backward/**",
        # We have to include all of /usr/lib/gcc/target/version/ b/c otherwise clang doesn't select this folder as a gcc installation.
        "usr/lib/gcc/{target_arch}-linux-{abi}/12/**",
    ]),
    visibility = ["//visibility:public"],
)

filegroup(
    name = "linker_files",
    srcs = glob([
        "lib/**",
        "lib64/**",
        "usr/lib/{target_arch}-linux-{abi}/**",
        "usr/lib/gcc/{target_arch}-linux-{abi}/**",
    ]),
    visibility = ["//visibility:public"],
)

filegroup(
    name = "tar",
    srcs = ["{tar_path}"],
    visibility = ["//visibility:public"],
)

sysroot_toolchain(
    name = "sysroot_toolchain",
    architecture = "{target_arch}",
    files = ":all_files",
    path = "{path_to_this_repo}",
    tar = ":tar",
)

toolchain(
    name = "toolchain",
    target_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:{target_arch}",
    ],
    target_settings = [
        "@px//bazel/cc_toolchains:libc_version_{libc_version}",
    ],
    toolchain = ":sysroot_toolchain",
    toolchain_type = "@px//bazel/cc_toolchains/sysroots/build:toolchain_type",
    visibility = ["//visibility:public"],
)
