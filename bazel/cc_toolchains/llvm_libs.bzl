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
load("//bazel/cc_toolchains/sysroots:sysroots.bzl", "sysroot_architectures", "sysroot_libc_versions")

def _llvm_variants():
    variants = []
    for arch in sysroot_architectures:
        libc_versions = sysroot_libc_versions
        if arch == "x86_64":
            libc_versions = libc_versions + ["glibc_host"]

        for libc_version in libc_versions:
            for use_libcpp in [True, False]:
                sanitizers = ["none"]

                # Only run sanitizers on host x86_64 build.
                if use_libcpp and arch == "x86_64" and libc_version == "glibc_host":
                    sanitizers = sanitizers + ["asan", "msan", "tsan"]

                for san in sanitizers:
                    variants.append((arch, libc_version, use_libcpp, san))

    return variants

def _llvm_variant_settings():
    for variant in _llvm_variants():
        arch, libc_version, use_libcpp, sanitizer = variant
        configs_to_match = [
            ":libc_version_" + libc_version,
            "@platforms//cpu:" + arch,
        ]
        if use_libcpp:
            configs_to_match.append("//bazel:use_libcpp")
            configs_to_match.append("//bazel:sanitizer_" + sanitizer)
        else:
            configs_to_match.append("//bazel:use_libstdcpp")

        selects.config_setting_group(
            name = _llvm_variant_setting_name(variant),
            match_all = configs_to_match,
            visibility = ["//visibility:public"],
        )

def _llvm_variant_setting_name(variant):
    arch, libc_version, use_libcpp, sanitizer = variant
    name = "llvm_variant_{arch}_{libc_version}".format(
        arch = arch,
        libc_version = libc_version,
    )
    if use_libcpp:
        name = name + "_libcpp"

    if sanitizer != "none":
        name = name + "_" + sanitizer
    return name

def _llvm_variant_setting_label(variant):
    name = _llvm_variant_setting_name(variant)
    return "@px//bazel/cc_toolchains:" + name

def _llvm_variant_repo_name(variant):
    arch, libc_version, use_libcpp, sanitizer = variant
    sanitizer_tag = "_" + sanitizer
    return "com_llvm_lib{libcpp}_{arch}_{libc_version}{sanitizer}".format(
        arch = arch,
        libc_version = libc_version.replace(".", "_"),
        libcpp = "_libcpp" if use_libcpp else "",
        sanitizer = sanitizer_tag if sanitizer != "none" else "",
    )

llvm_variant_settings = _llvm_variant_settings
llvm_variant_setting_label = _llvm_variant_setting_label
llvm_variant_repo_name = _llvm_variant_repo_name
llvm_variants = _llvm_variants
