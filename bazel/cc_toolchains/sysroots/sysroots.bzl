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

load("@bazel_skylib//lib:selects.bzl", "selects")
load("//bazel/cc_toolchains:utils.bzl", "abi")

SYSROOT_LOCATIONS = dict(
    sysroot_x86_64_glibc2_36_runtime = dict(
        sha256 = "7dcdec4bd693329a1ed670a87e95814a87f0906bd0f0c14058f47003b9fcf52a",
        strip_prefix = "",
        urls = ["https://storage.googleapis.com/pixie-dev-public/sysroots/pl6/sysroot-amd64-runtime.tar.gz"],
    ),
    sysroot_x86_64_glibc2_36_build = dict(
        sha256 = "b3c7cd7044ba3de0f3550bece6e21f69eb198e9a165eeab690257a749fe442d0",
        strip_prefix = "",
        urls = ["https://storage.googleapis.com/pixie-dev-public/sysroots/pl6/sysroot-amd64-build.tar.gz"],
    ),
    sysroot_x86_64_glibc2_36_test = dict(
        sha256 = "bee652932886a34bbd5bd9ea3a3c52c08f462c005faa0e2353297dd5b9070989",
        strip_prefix = "",
        urls = ["https://storage.googleapis.com/pixie-dev-public/sysroots/pl6/sysroot-amd64-test.tar.gz"],
    ),
    sysroot_x86_64_glibc2_36_debug = dict(
        sha256 = "5639c73b5afd7c16bbdcbfcbd54262ea232283a5f5b4852e51c77062dea62f2e",
        strip_prefix = "",
        urls = ["https://storage.googleapis.com/pixie-dev-public/sysroots/pl6/sysroot-amd64-debug.tar.gz"],
    ),
    sysroot_aarch64_glibc2_36_runtime = dict(
        sha256 = "48ea0856326974dca562db5a4e7ef09befef50fdb7a13698966e06393a3b090c",
        strip_prefix = "",
        urls = ["https://storage.googleapis.com/pixie-dev-public/sysroots/pl6/sysroot-arm64-runtime.tar.gz"],
    ),
    sysroot_aarch64_glibc2_36_build = dict(
        sha256 = "304e86332754e473938d5614a12e8676fb36481096ab6915b7d667faa0373a74",
        strip_prefix = "",
        urls = ["https://storage.googleapis.com/pixie-dev-public/sysroots/pl6/sysroot-arm64-build.tar.gz"],
    ),
    sysroot_aarch64_glibc2_36_test = dict(
        sha256 = "12be586d2562e1ed49e4ea5c178c8df73c596c522b75b9b1de1700e081c6fd1e",
        strip_prefix = "",
        urls = ["https://storage.googleapis.com/pixie-dev-public/sysroots/pl6/sysroot-arm64-test.tar.gz"],
    ),
    sysroot_aarch64_glibc2_36_debug = dict(
        sha256 = "4cb30d6f9e73f66a03ba34a74fb51171ac7d45365102c41db2add0b852c7db33",
        strip_prefix = "",
        urls = ["https://storage.googleapis.com/pixie-dev-public/sysroots/pl6/sysroot-arm64-debug.tar.gz"],
    ),
)

_sysroot_architectures = ["aarch64", "x86_64"]
_sysroot_libc_versions = ["glibc2_36"]
_sysroot_variants = ["runtime", "build", "test", "debug"]

def _sysroot_repo_name(target_arch, libc_version, variant):
    name = "sysroot_{target_arch}_{libc_version}_{variant}".format(
        target_arch = target_arch,
        libc_version = libc_version,
        variant = variant,
    )
    if name in SYSROOT_LOCATIONS:
        return name
    return ""

def _sysroot_setting_name(target_arch, libc_version):
    return "using_sysroot_{target_arch}_{libc_version}".format(
        target_arch = target_arch,
        libc_version = libc_version,
    )

def _sysroot_repo_impl(rctx):
    loc = SYSROOT_LOCATIONS[rctx.attr.name]
    tar_path = "sysroot.tar.gz"
    rctx.download(
        url = loc["urls"],
        output = tar_path,
        sha256 = loc["sha256"],
    )

    rctx.extract(
        tar_path,
        stripPrefix = loc.get("strip_prefix", ""),
    )

    rctx.template(
        "BUILD.bazel",
        Label("@px//bazel/cc_toolchains/sysroots/{variant}:sysroot.BUILD".format(variant = rctx.attr.variant)),
        substitutions = {
            "{abi}": abi(rctx.attr.target_arch, rctx.attr.libc_version),
            "{libc_version}": rctx.attr.libc_version,
            "{path_to_this_repo}": "external/" + rctx.attr.name,
            "{tar_path}": tar_path,
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
    fields = ["files", "architecture", "path", "tar"],
)

def _sysroot_toolchain_impl(ctx):
    return [
        platform_common.ToolchainInfo(
            sysroot = SysrootInfo(
                files = ctx.attr.files.files,
                architecture = ctx.attr.architecture,
                path = ctx.attr.path,
                tar = ctx.attr.tar.files,
            ),
        ),
    ]

sysroot_toolchain = rule(
    implementation = _sysroot_toolchain_impl,
    attrs = {
        "architecture": attr.string(mandatory = True, doc = "CPU architecture targeted by this sysroot"),
        "files": attr.label(mandatory = True, doc = "All sysroot files"),
        "path": attr.string(mandatory = True, doc = "Path to sysroot relative to execroot"),
        "tar": attr.label(mandatory = True, doc = "Sysroot tar, used to avoid repacking the sysroot as a tar for docker images."),
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

def _pl_sysroot_settings():
    for target_arch in _sysroot_architectures:
        for libc_version in _sysroot_libc_versions:
            selects.config_setting_group(
                name = _sysroot_setting_name(target_arch, libc_version),
                match_all = [
                    "@platforms//cpu:" + target_arch,
                    "//bazel/cc_toolchains:libc_version_" + libc_version,
                ],
                visibility = ["//visibility:public"],
            )

sysroot_repo_name = _sysroot_repo_name
sysroot_libc_versions = _sysroot_libc_versions
sysroot_architectures = _sysroot_architectures
pl_sysroot_settings = _pl_sysroot_settings
pl_sysroot_deps = _pl_sysroot_deps
