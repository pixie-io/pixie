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

load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "common_headers",
    srcs = glob(
        ["include/*.h"],
    ),
    hdrs = glob(["include/*.h"]),
    defines = [
        "HAVE_NANOSLEEP",
        "HAVE_LIBUUID",
        "HAVE_UNISTD_H",
        "HAVE_LIBSTD_H",
        "HAVE_SYS_TIME_H",
        "HAVE_SYS_FILE_H",
        "HAVE_SYS_IOCTL_H",
        "HAVE_SYS_SOCKET_H",
    ],
    strip_include_prefix = "include",
)

cc_library(
    name = "uuid",
    srcs = glob(
        [
            "libuuid/src/*.c",
            "libuuid/src/*.h",
            "lib/*.c",
        ],
        exclude = [
            "libuuid/src/test_uuid.c",
            "libuuid/src/uuid_time.c",
        ],
    ),
    hdrs = [
        "libuuid/src/uuid.h",
    ],
    strip_include_prefix = "libuuid/src",
    visibility = ["//visibility:public"],
    deps = [":common_headers"],
)
