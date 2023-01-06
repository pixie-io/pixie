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

load("//bazel/cc_toolchains:clang.bzl", "clang_register_toolchain")

def _pl_register_cc_toolchains():
    clang_register_toolchain(
        name = "clang-15.0",
        toolchain_repo = "com_llvm_clang_15",
        libcxx_repo = "com_llvm_libcxx",
        target_arch = "x86_64",
        clang_version = "15.0.6",
    )
    clang_register_toolchain(
        name = "clang-15.0-exec",
        toolchain_repo = "com_llvm_clang_15",
        libcxx_repo = "com_llvm_libcxx",
        target_arch = "x86_64",
        clang_version = "15.0.6",
        use_for_host_tools = True,
    )

    native.register_toolchains(
        "//bazel/cc_toolchains:cc-toolchain-gcc-x86_64-gnu",
        "//bazel/cc_toolchains:cc-toolchain-gcc-x86_64-static-musl",
    )

pl_register_cc_toolchains = _pl_register_cc_toolchains
