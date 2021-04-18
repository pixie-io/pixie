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

# This macro preprocesses BPF C files to include any user-includes.
# It essentially does this by running the C preprocessor.
# BPF source should not, however, have system includes inlined, as that
# should be done on the host system where the BPF code is deployed.
# There is, unfortunately no way to tell the C preprocessor to process user includes only.
# To fake this, there is a set of mock system headers that is defined,
# which serve as an indirection so the includes stay in place.
# The mock system headers take the following form:
# file: linux/sched.h
# contents: include <linux/sched.h>
#
# @param src: The location of the BPF source code.
# @param hdrs: Any user defined headers required by the BPF program (src).
# @param syshdrs: The location of the fake system headers to bypass system includes.
#                 The syshdrs variable must be a path directory (i.e. not a target with a colon).
#                 For example, "//src/stirling/bpf_tools/bcc_bpf/system-headers",
#                 instead of "//src/stirling/bpf_tools/bcc_bpf/system-headers:system-headers".
#                 To make this work, the directory must have a BUILD.bazel with a target
#                 that has the same name as the directory.
#                 This enables automatic generation of the preprocessor -I flag internally.
def pl_bpf_preprocess(
        name,
        src,
        hdrs,
        syshdrs,
        tags = [],
        *kwargs):
    out_file = name

    # Hacky: Extract the name of the directory by removing the leading "//".
    # There might be Bazel-esque way of doing this, but since filegroups can't have $location
    # applied to it, we use this hack instead.
    # For more details, see note about syshdrs above.
    syshdrs_dir = syshdrs[2:]

    cmd = "cpp -U linux -Dinclude=#include -I. -I{} $(location {}) -o $@".format(syshdrs_dir, src)

    native.genrule(
        name = name + "_preprocess_rule",
        outs = [out_file],
        srcs = [src] + hdrs + [syshdrs],
        tags = tags,
        cmd = cmd,
    )
    return out_file
