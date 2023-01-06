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

def add_llvm_cache_entries(cache_entries):
    return select({
        "@px//bazel:use_libcpp": dict(
            cache_entries,
            LLVM_ROOT = "$EXT_BUILD_ROOT/external/com_llvm_lib_libcpp",
        ),
        "//conditions:default": dict(
            cache_entries,
            LLVM_ROOT = "$EXT_BUILD_ROOT/external/com_llvm_lib",
        ),
    })

def llvm_build_data_deps():
    return select({
        "@px//bazel:use_libcpp": ["@com_llvm_lib_libcpp//:cmake"],
        "//conditions:default": ["@com_llvm_lib//:cmake"],
    })
