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

kernel_build_date = 20230228151027
kernel_catalog = {
    "4.14.304": "58c474a6f3cf0c93af39d0549ddcb3e36b852026c42229fc6d4a40260e34329d",
    "5.15.90": "2960730ead8f9b20e97e68b1148ea5e208ecebcadc04a2bef99a5936a3d525fa",
    "5.18.19": "3da6271916d253810e4ef2e587dd426ad1f9d1bbb98b5624994c378604e3de23",
    "6.1.8": "767aa4c6936330512de3c38e4b2036181a56d7d01158947c7ee8747402bd479c",
}

def kernel_version_to_name(version):
    return "linux_build_{}_x86_64".format(version.replace(".", "_"))

def qemu_kernel_deps():
    for version, sha in kernel_catalog.items():
        http_file(
            name = kernel_version_to_name(version),
            url = "https://storage.googleapis.com/pixie-dev-public/kernel-build/20230228151027/linux-build-{}.tar.gz".format(version),
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

def qemu_image_to_deps():
    # Returns a dict which has the kernel names, deps.
    deps = {}

    def get_dep_name(version):
        return "@{}//file:linux-build.tar.gz".format(kernel_version_to_name(version))

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
