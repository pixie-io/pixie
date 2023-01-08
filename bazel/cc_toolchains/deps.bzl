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

load("//bazel:repository_locations.bzl", "REPOSITORY_LOCATIONS")

def _config_repo_impl(repository_ctx):
    repository_ctx.download(
        url = repository_ctx.attr.urls,
        output = "cc_toolchain_config.bzl",
        sha256 = repository_ctx.attr.sha256,
    )
    repository_ctx.file("BUILD.bazel", content = 'exports_files(["cc_toolchain_config.bzl"])')
    repository_ctx.symlink(repository_ctx.attr.toolchain_features, "toolchain_features.bzl")
    repository_ctx.patch(repository_ctx.attr.patch, strip = 1)

cc_toolchain_config_repository = repository_rule(
    implementation = _config_repo_impl,
    attrs = dict(
        urls = attr.string_list(mandatory = True),
        sha256 = attr.string(mandatory = True),
        patch = attr.label(mandatory = True),
        toolchain_features = attr.label(mandatory = True),
    ),
)

def cc_toolchain_config_repo(name, patch):
    loc = REPOSITORY_LOCATIONS[name]
    cc_toolchain_config_repository(
        name = "unix_cc_toolchain_config",
        urls = loc["urls"],
        sha256 = loc["sha256"],
        patch = patch,
        toolchain_features = "//bazel/cc_toolchains:toolchain_features.bzl",
    )
