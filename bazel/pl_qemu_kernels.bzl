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

kernel_build_date = 20230620214456
kernel_catalog = {
    "4.14.254": "857030b52d0a48bc95ab544439cf52da07bce8cc2b9bef2e0382f02c182fab78",
    "4.19.254": "5203a52b48a966ddfc8564ed2d0fd1e84f20ef417cbb5d41eb0b1c8413e4853d",
    "5.10.173": "8c746c5ee94964323ca69533950370f595426efd671bfed85473e5a4b570ec26",
    "5.15.101": "e18b8c5e91e3856dd6705a324ec7ebb2f1be5ff503c1322f5ddc6c44255272e4",
    "5.4.235": "828f69979065800a4f214a1be369ab9756fdc2c586bd5bbfcc6d2a0f501601d5",
    "6.1.18": "31f125451a70249e206a1eba555e432d5e08d173a4ab9d675f0b5d57f879f81b",
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
