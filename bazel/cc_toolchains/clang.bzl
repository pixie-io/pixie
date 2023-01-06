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

def _download_repo(rctx, repo_name, output):
    loc = REPOSITORY_LOCATIONS[repo_name]
    rctx.download_and_extract(
        output = output,
        url = loc["urls"],
        sha256 = loc["sha256"],
        stripPrefix = loc.get("strip_prefix", ""),
    )

def _clang_toolchain_impl(rctx):
    # Unfortunately, we have to download any files that the toolchain uses within this rule.
    toolchain_path = "toolchain"
    _download_repo(rctx, rctx.attr.toolchain_repo, toolchain_path)
    libcxx_path = "libcxx"
    _download_repo(rctx, rctx.attr.libcxx_repo, libcxx_path)

    libcxx_build = rctx.read(Label("@px//bazel/cc_toolchains/clang:libcxx.BUILD"))
    toolchain_files_build = rctx.read(Label("@px//bazel/cc_toolchains/clang:toolchain_files.BUILD"))

    # First combine all of the build file templates into one file.
    rctx.template(
        "BUILD.bazel.tpl",
        Label("@px//bazel/cc_toolchains/clang:toolchain.BUILD"),
        substitutions = {
            "{libcxx_build}": libcxx_build,
            "{toolchain_files_build}": toolchain_files_build,
        },
    )

    # Then substitute in parameters into the combined template.
    rctx.template(
        "BUILD.bazel",
        "BUILD.bazel.tpl",
        substitutions = {
            "{clang_major_version}": rctx.attr.clang_version.split(".")[0],
            "{clang_version}": rctx.attr.clang_version,
            "{host_arch}": rctx.attr.host_arch,
            "{host_libc_version}": rctx.attr.host_libc_version,
            "{libc_version}": rctx.attr.libc_version,
            "{libcxx_path}": libcxx_path,
            "{target_arch}": rctx.attr.target_arch,
            "{this_repo}": rctx.attr.name,
            "{toolchain_path}": toolchain_path,
            "{use_for_host_tools}": str(rctx.attr.use_for_host_tools),
        },
    )

clang_toolchain = repository_rule(
    _clang_toolchain_impl,
    attrs = dict(
        toolchain_repo = attr.string(mandatory = True),
        libcxx_repo = attr.string(mandatory = True),
        target_arch = attr.string(mandatory = True),
        libc_version = attr.string(mandatory = True),
        host_arch = attr.string(mandatory = True),
        host_libc_version = attr.string(mandatory = True),
        clang_version = attr.string(mandatory = True),
        use_for_host_tools = attr.bool(mandatory = True),
    ),
)

def _clang_register_toolchain(
        name,
        toolchain_repo,
        libcxx_repo,
        target_arch,
        clang_version,
        libc_version = "gnu",
        host_arch = "x86_64",
        host_libc_version = "gnu",
        use_for_host_tools = False):
    clang_toolchain(
        name = name,
        toolchain_repo = toolchain_repo,
        libcxx_repo = libcxx_repo,
        target_arch = target_arch,
        libc_version = libc_version,
        host_arch = host_arch,
        host_libc_version = host_libc_version,
        clang_version = clang_version,
        use_for_host_tools = use_for_host_tools,
    )
    native.register_toolchains("@{name}//:toolchain".format(name = name))

clang_register_toolchain = _clang_register_toolchain
