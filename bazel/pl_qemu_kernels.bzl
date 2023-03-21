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

kernel_build_date = 20230316174254
kernel_catalog = {
    "4.14.254": "03c7a7f030026e403a3fc524f6207e9a417d0855b039fd302aa3fa0535e106df",
    "4.19.254": "3e25938a654cd2115e26e3034de4488a8e6d493df08ac09ed4c3330a1ef34a8a",
    "5.10.173": "a7b863fd2c6d3af242b16dd479cce4013feba6c5365a73bc28f20116f69579b4",
    "5.15.101": "a30863b9b65583ebd8c90ef4e4446b949301c7a834e03bbae0eb2d6211eb1f9b",
    "5.4.235": "30393414aec420d23986b0bf98eb5c2cf430576d748d425b4d683fa2d09fcd75",
    "6.1.18": "2578e77017157b9fd004242dfba5aec233c7fce87c4e4e55d7dff2d118021fc3",
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
