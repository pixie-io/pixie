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

# Linux headers are bundled in PEM image for PEM to setup the BPF runtime.
def linux_headers():
    # timeconst_<config_hz>.h headers are used by BCC runtime for compilation. But it's part of
    # generated headers not in the original linux source. So we pack them into separately.
    http_file(
        name = "timeconst_100",
        urls = ["https://storage.googleapis.com/pixie-dev-public/timeconst_100.h"],
        sha256 = "082496c45ab93af811732da56000caf5ffc9e6734ff633a2b348291f160ceb7e",
        downloaded_file_path = "timeconst_100.h",
    )

    http_file(
        name = "timeconst_250",
        urls = ["https://storage.googleapis.com/pixie-dev-public/timeconst_250.h"],
        sha256 = "0db01d74b846e39dca3612d96dee8b8f6addfaeb738cc4f5574086828487c2b9",
        downloaded_file_path = "timeconst_250.h",
    )

    http_file(
        name = "timeconst_300",
        urls = ["https://storage.googleapis.com/pixie-dev-public/timeconst_300.h"],
        sha256 = "91c6499df71695699a296b2fdcbb8c30e9bf35d024e048fa6d2305a8ac2af9ab",
        downloaded_file_path = "timeconst_300.h",
    )

    http_file(
        name = "timeconst_1000",
        urls = ["https://storage.googleapis.com/pixie-dev-public/timeconst_1000.h"],
        sha256 = "da0ba6765f2969482bf8eaf21249552557fe4d6831749d9cfe4c25f4661f8726",
        downloaded_file_path = "timeconst_1000.h",
    )

    http_file(
        name = "linux_headers_merged_tar_gz",
        urls = ["https://storage.googleapis.com/pixie-dev-public/linux-headers/pl4/linux-headers-merged-pl4.tar.gz"],
        sha256 = "484e3ddf21bf4474c34f17e5862758cc3552045cc9a1dd746203518eb298d876",
        downloaded_file_path = "linux-headers-merged.tar.gz",
    )
