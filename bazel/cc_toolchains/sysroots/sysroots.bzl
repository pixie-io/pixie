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

load("//bazel/cc_toolchains:utils.bzl", "abi")

SYSROOT_LOCATIONS = dict(
    sysroot_x86_64_glibc2_36_runtime = dict(
        sha256 = "85db7221f1334a802fb7f3651b2e27f596ddeeb0869d22678f0624cd9947b929",
        strip_prefix = "",
        urls = ["https://storage.googleapis.com/pixie-dev-public/sysroots/pl1/sysroot-amd64-runtime.tar.gz"],
    ),
    sysroot_x86_64_glibc2_36_build = dict(
        sha256 = "150d0592aca950668330c01981fccef37ab2ac990fd66a5ec36359d54e95a13c",
        strip_prefix = "",
        urls = ["https://storage.googleapis.com/pixie-dev-public/sysroots/pl1/sysroot-amd64-build.tar.gz"],
    ),
    sysroot_x86_64_glibc2_36_test = dict(
        sha256 = "a4e4dcfbfecef8710576fd977fb7f3d3a6bda4ebca526b411e4fda78870593d1",
        strip_prefix = "",
        urls = ["https://storage.googleapis.com/pixie-dev-public/sysroots/pl1/sysroot-amd64-test.tar.gz"],
    ),
    sysroot_aarch64_glibc2_36_runtime = dict(
        sha256 = "84d102939126a557dcb6bad2d0c4a86f526b09f0e247b35544b9975a12aa7a98",
        strip_prefix = "",
        urls = ["https://storage.googleapis.com/pixie-dev-public/sysroots/pl1/sysroot-arm64-runtime.tar.gz"],
    ),
    sysroot_aarch64_glibc2_36_build = dict(
        sha256 = "e61c8fa3e58a0f3b83145a807b13401cfe693c0672097cbbe125e68d92e7b0e9",
        strip_prefix = "",
        urls = ["https://storage.googleapis.com/pixie-dev-public/sysroots/pl1/sysroot-arm64-build.tar.gz"],
    ),
    sysroot_aarch64_glibc2_36_test = dict(
        sha256 = "2bdd9a21d3b6a1ba05e63fa28235867be053748963fbe617376ad5b8d3592e4b",
        strip_prefix = "",
        urls = ["https://storage.googleapis.com/pixie-dev-public/sysroots/pl1/sysroot-arm64-test.tar.gz"],
    ),
)

_sysroot_architectures = ["aarch64", "x86_64"]
_sysroot_libc_versions = ["glibc2_36"]
_sysroot_variants = ["runtime", "build", "test"]

def _sysroot_repo_name(target_arch, libc_version, variant):
    name = "sysroot_{target_arch}_{libc_version}_{variant}".format(
        target_arch = target_arch,
        libc_version = libc_version,
        variant = variant,
    )
    if name in SYSROOT_LOCATIONS:
        return name
    return ""

def _sysroot_repo_impl(rctx):
    loc = SYSROOT_LOCATIONS[rctx.attr.name]
    rctx.download_and_extract(
        url = loc["urls"],
        sha256 = loc["sha256"],
        stripPrefix = loc.get("strip_prefix", ""),
    )
    rctx.template(
        "BUILD.bazel",
        Label("@px//bazel/cc_toolchains/sysroots/{variant}:sysroot.BUILD".format(variant = rctx.attr.variant)),
        substitutions = {
            "{abi}": abi(rctx.attr.target_arch, rctx.attr.libc_version),
            "{libc_version}": rctx.attr.libc_version,
            "{path_to_this_repo}": "external/" + rctx.attr.name,
            "{target_arch}": rctx.attr.target_arch,
        },
    )

_sysroot_repo = repository_rule(
    implementation = _sysroot_repo_impl,
    attrs = {
        "libc_version": attr.string(mandatory = True, doc = "Libc version of the sysroot"),
        "target_arch": attr.string(mandatory = True, doc = "CPU Architecture of the sysroot"),
        "variant": attr.string(mandatory = True, doc = "Use case variant of the sysroot. One of 'runtime', 'build', or 'test'"),
    },
)

SysrootInfo = provider(
    doc = "Information about a sysroot.",
    fields = ["files", "architecture", "path"],
)

def _sysroot_toolchain_impl(ctx):
    return [
        platform_common.ToolchainInfo(
            sysroot = SysrootInfo(
                files = ctx.attr.files.files,
                architecture = ctx.attr.architecture,
                path = ctx.attr.path,
            ),
        ),
    ]

sysroot_toolchain = rule(
    implementation = _sysroot_toolchain_impl,
    attrs = {
        "architecture": attr.string(mandatory = True, doc = "CPU architecture targeted by this sysroot"),
        "files": attr.label(mandatory = True, doc = "All sysroot files"),
        "path": attr.string(mandatory = True, doc = "Path to sysroot relative to execroot"),
    },
)

def _pl_sysroot_deps():
    toolchains = []
    for target_arch in _sysroot_architectures:
        for libc_version in _sysroot_libc_versions:
            for variant in _sysroot_variants:
                repo = _sysroot_repo_name(target_arch, libc_version, variant)
                _sysroot_repo(
                    name = repo,
                    target_arch = target_arch,
                    libc_version = libc_version,
                    variant = variant,
                )
                toolchains.append("@{repo}//:toolchain".format(repo = repo))
    native.register_toolchains(*toolchains)

sysroot_repo_name = _sysroot_repo_name
sysroot_libc_versions = _sysroot_libc_versions
sysroot_architectures = _sysroot_architectures
pl_sysroot_deps = _pl_sysroot_deps
