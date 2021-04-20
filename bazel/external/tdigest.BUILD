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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

licenses(["notice"])

package(default_visibility = ["//visibility:public"])

exports_files(["LICENSE"])

cc_library(
    name = "tdigest",
    srcs = [
        "tdigest.h",
    ],
    hdrs = [
        "tdigest.h",
    ],
    copts = [
        "-std=c++17",
    ],
    include_prefix = "tdigest",
)

cc_test(
    name = "tdigest_test",
    size = "small",
    srcs = [
        "tdigest_test.cpp",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        ":tdigest",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
    ],
)
