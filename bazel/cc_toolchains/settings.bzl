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

load("@bazel_skylib//rules:common_settings.bzl", "string_flag")
load("//bazel/cc_toolchains/sysroots:sysroots.bzl", "sysroot_libc_versions")

HOST_GLIBC_VERSION = "glibc_host"

def _settings():
    libc_versions = sysroot_libc_versions + [
        HOST_GLIBC_VERSION,
    ]
    string_flag(
        name = "libc_version",
        build_setting_default = HOST_GLIBC_VERSION,
        values = libc_versions,
    )

    for version in libc_versions:
        native.config_setting(
            name = "libc_version_" + version,
            flag_values = {
                "//bazel/cc_toolchains:libc_version": version,
            },
        )

    string_flag(
        name = "compiler",
        build_setting_default = "clang",
        values = [
            "clang",
            "gcc",
        ],
        visibility = [
            "//visibility:public",
        ],
    )
    native.config_setting(
        name = "compiler_clang",
        flag_values = {
            "//bazel/cc_toolchains:compiler": "clang",
        },
    )
    native.config_setting(
        name = "compiler_gcc",
        flag_values = {
            "//bazel/cc_toolchains:compiler": "gcc",
        },
    )

settings = _settings
