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

load("@io_bazel_rules_go//go:def.bzl", "go_binary")

# Allow building a go binary with specified go SDK. Used to verify Pixie's compatibility with
# different golang SDK versions. For example, verify Pixie can handle different go ABIs.
def _go_versioned_binary_impl(name, embed, sdk):
    go_binary(
        name = name,
        embed = embed,
        # -N -l are set by default in debug builds, and go tool compile re-enables inlining
        # if it sees `-l -l`, i.e., options are toggled each time it's specified.
        # According to go tool compile -help
        # -N	disable optimizations
        # -l	disable inlining
        # This makes sure the function symbol address stable across builds.
        gc_goopts = select({
            "//bazel:debug_build": [],
            "//conditions:default": [
                "-N",
                "-l",
            ],
        }),
        testonly = True,
        goarch = "amd64",
        goos = "linux",
        gosdk = sdk,
    )

def go_1_16_binary(name, embed):
    _go_versioned_binary_impl(name, embed, "@go_sdk_1_16//:go_sdk")

def go_1_17_binary(name, embed):
    _go_versioned_binary_impl(name, embed, "@go_sdk_1_17//:go_sdk")
