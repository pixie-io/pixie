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

load("@com_github_fmeum_rules_meta//meta:defs.bzl", "meta")
load("//bazel/test_runners/qemu_with_kernel:runner.bzl", "qemu_with_kernel_interactive_runner")

java_graal_binary = meta.wrap_with_transition(
    native.java_binary,
    {
        "java_runtime_version": meta.replace_with("remotejdk_openjdk_graal_17"),
    },
    executable = True,
)

cc_clang_binary = meta.wrap_with_transition(
    native.cc_binary,
    {
        "@//bazel/cc_toolchains:compiler": meta.replace_with("clang"),
    },
    executable = True,
)

qemu_interactive_runner = meta.wrap_with_transition(
    qemu_with_kernel_interactive_runner,
    {
        "@//bazel/cc_toolchains:libc_version": meta.replace_with("glibc2_36"),
    },
    executable = True,
)
