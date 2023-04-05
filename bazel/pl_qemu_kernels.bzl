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

kernel_build_date = 20230406145001
kernel_catalog = {
    "4.14.254": "47e9159d229694f8a78a0fceb8acf4438d345c6a62fb6c99337893d6b76f18a4",
    "4.19.254": "78d7d59d30d3eed06abb10754985e1b4abb54b652b62f79e6f81b6648efd134a",
    "5.10.173": "7043331f05cbd1023a84ad366eb662556963557b3da93ea651ab3a38410dfb5e",
    "5.15.101": "4e68bef1ac3ee5f696683b6ffceb5f8042427942851f54dda4eeaa26696267cf",
    "5.4.235": "65a568ed5d4d1c5ad9502794402a8ca0a821315bd643cd0e8aa064c94df6ebe9",
    "6.1.18": "b2f4a134d7c4a84a129b7b977f54740dceeb48233b8beeefcd86be04a531dcde",
}

def kernel_version_to_name(version):
    return "linux_build_{}_x86_64".format(version.replace(".", "_"))

def qemu_kernel_deps():
    for version, sha in kernel_catalog.items():
        http_file(
            name = kernel_version_to_name(version),
            url = "https://storage.googleapis.com/pixie-dev-public/kernel-build/{}/linux-build-{}.tar.gz".format(kernel_build_date, version),
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
