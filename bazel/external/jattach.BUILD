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

load("@rules_foreign_cc//foreign_cc:defs.bzl", "make")

filegroup(
    name = "jattach-sources",
    srcs = ["Makefile"] + glob(["src/posix/**"]),
    visibility = ["//visibility:public"],
)

make(
    name = "jattach",
    make_commands = ["make install"],
    out_static_libs = ["jattach.a"],
    lib_source = ":jattach-sources",
    visibility = ["//visibility:public"],
)
