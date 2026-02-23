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

load("@with_cfg.bzl", "with_cfg")
load("//bazel/test_runners/qemu_with_kernel:runner.bzl", "qemu_with_kernel_interactive_runner")

java_graal_binary, _java_graal_binary_internal = with_cfg(native.java_binary).set(
    "java_runtime_version", "remotejdk_openjdk_graal_17").build()

cc_clang_binary, _cc_clang_binary_internal = with_cfg(native.cc_binary).set(
        Label("@//bazel/cc_toolchains:compiler"), "clang").set(
        Label("@//bazel/cc_toolchains:libc_version"), "glibc2_36").build()

qemu_interactive_runner, _qemu_interactive_runner_internal = with_cfg(qemu_with_kernel_interactive_runner).set(
    Label("@//bazel/cc_toolchains:libc_version"), "glibc2_36").build()
