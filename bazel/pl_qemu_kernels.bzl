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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")

kernel_build_date = 20240826223635
kernel_catalog = {
    "4.14.254": "007c1d9d7385c7aa9df8434b3c2a2c38963c88391d9f4b88d5ac67bbccbbccef",
    "4.19.254": "946d914990ae5b2f6dae6ac0ecbc81c1f63df84832ecd7563c18e7dc99986d2f",
    "5.4.254": "fbae23c3da8af04c2a039bff7a5926f71041506cd150d02209f1b1b10cd38329",
    "5.10.224": "d5b3cd7282f0533fe1e2574a73f9164bc90fae1098fe0284cf37a2995ce0e762",
    "5.15.165": "0b70925e4497d74d99fd43544b259c5ed2dbc4c91c4cf702f872698a2bf7d80d",
    "6.1.106": "fa03bd44bd6a5a7e1dcf1ee6a1790add4f2297d85cfffdc61b52f123eda27dc7",
    "6.8.12": "06db1941423384c4b956d1e7a46a888a5647d15504ee31fe8ae8bdff264f1678",
}


def kernel_version_to_name(version):
    return "linux_build_{}_x86_64".format(version.replace(".", "_"))

def qemu_kernel_deps():
    for version, sha in kernel_catalog.items():
        http_file(
            name = kernel_version_to_name(version),
            urls = [
                "https://github.com/pixie-io/dev-artifacts/releases/download/kernel-build%2F{}/linux-build-{}.tar.gz".format(kernel_build_date, version),
                "https://storage.googleapis.com/pixie-dev-public/kernel-build/{}/linux-build-{}.tar.gz".format(kernel_build_date, version),
            ],
            sha256 = sha,
            downloaded_file_path = "linux-build.tar.gz",
        )

def qemu_kernel_list():
    # Returns a list of kernels, including "oldest" and "latest".
    return ["oldest"] + kernel_catalog.keys() + ["latest"]

def _kernel_sort_key(version):
    # This assumes that there are no more than 1024 minor or patch versions.
    maj, minor, patch = version.split(".")
    return (int(maj) * (1024 * 1024)) + (int(minor) * (1024)) + int(patch)

def get_dep_name(version):
    return "@{}//file:linux-build.tar.gz".format(kernel_version_to_name(version))

def qemu_image_to_deps():
    # Returns a dict which has the kernel names, deps.
    deps = {}

    for version in kernel_catalog.keys():
        deps[version] = get_dep_name(version)

    # We need to add the oldest and latest versions.
    kernels_sorted = sorted(kernel_catalog.keys(), key = _kernel_sort_key)
    kernel_oldest = kernels_sorted[0]
    kernel_latest = kernels_sorted[-1]
    deps["oldest"] = get_dep_name(kernel_oldest)
    deps["latest"] = get_dep_name(kernel_latest)

    return deps

def kernel_flag_name(version):
    return "kernel_version_{}".format(version.replace(".", "_"))
