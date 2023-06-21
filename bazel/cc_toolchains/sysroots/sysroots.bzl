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
        sha256 = "0f6c8147394c41a5c10715d0859ca2ef6e03d65a52b5959f6e11f1de7513f3a4",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/sysroots%2Fpl8/sysroot-amd64-runtime.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/sysroots/pl8/sysroot-amd64-runtime.tar.gz",
        ],
    ),
    sysroot_x86_64_glibc2_36_build = dict(
        sha256 = "ccf1c8426baa180c8b1b178b58acf9c06c7e7b16383cf92f42719a2a05585058",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/sysroots%2Fpl8/sysroot-amd64-build.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/sysroots/pl8/sysroot-amd64-build.tar.gz",
        ],
    ),
    sysroot_x86_64_glibc2_36_test = dict(
        sha256 = "226b41c76e55899e9cc3bcbfed4646a610df5102dd9358add9450c80c5a14336",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/sysroots%2Fpl8/sysroot-amd64-test.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/sysroots/pl8/sysroot-amd64-test.tar.gz",
        ],
    ),
    sysroot_x86_64_glibc2_36_debug = dict(
        sha256 = "331d6c258a4abf5b877d93ae1d672fdd8758f5dafd7e614e19c5c20916ca1585",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/sysroots%2Fpl8/sysroot-amd64-debug.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/sysroots/pl8/sysroot-amd64-debug.tar.gz",
        ],
    ),
    sysroot_aarch64_glibc2_36_runtime = dict(
        sha256 = "2974c29dd6baef1416d5cc2781306823885e64acaf0ce8bd3b447929b6103f4e",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/sysroots%2Fpl8/sysroot-arm64-runtime.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/sysroots/pl8/sysroot-arm64-runtime.tar.gz",
        ],
    ),
    sysroot_aarch64_glibc2_36_build = dict(
        sha256 = "04b957f6be840372e54075b966f76ea4238294028620ac8f8257c9e0fdb15bef",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/sysroots%2Fpl8/sysroot-arm64-build.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/sysroots/pl8/sysroot-arm64-build.tar.gz",
        ],
    ),
    sysroot_aarch64_glibc2_36_test = dict(
        sha256 = "59934822ae0ffe1e990fba589a94651eb92d0bddf2e40419f81299cf600eb62f",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/sysroots%2Fpl8/sysroot-arm64-test.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/sysroots/pl8/sysroot-arm64-test.tar.gz",
        ],
    ),
    sysroot_aarch64_glibc2_36_debug = dict(
        sha256 = "910d4ba853dedb6b9fcd05c862ed3e4a933cea941334064b7b62f8ae7fe4b87d",
        strip_prefix = "",
        urls = [
            "https://github.com/pixie-io/dev-artifacts/releases/download/sysroots%2Fpl8/sysroot-arm64-debug.tar.gz",
            "https://storage.googleapis.com/pixie-dev-public/sysroots/pl8/sysroot-arm64-debug.tar.gz",
        ],
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
