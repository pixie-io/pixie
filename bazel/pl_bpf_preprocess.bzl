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

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain")

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
def _pl_bpf_preprocess_impl(ctx):
    if len(ctx.attr.src.files.to_list()) != 1:
        fail("Must provide exactly one file to 'src' attr of pl_bpf_preprocess")

    cc_toolchain = find_cpp_toolchain(ctx)

    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        requested_features = ctx.features,
        unsupported_features = ctx.disabled_features,
    )

    preprocessor_tool = cc_common.get_tool_for_action(
        feature_configuration = feature_configuration,
        action_name = "c-preprocess",
    )

    output = ctx.actions.declare_file(ctx.attr.name + ".c")

    merged_cc_info = cc_common.merge_cc_infos(direct_cc_infos = [dep[CcInfo] for dep in ctx.attr.deps])
    compilation_ctx = merged_cc_info.compilation_context

    includes = compilation_ctx.includes.to_list() + compilation_ctx.quote_includes.to_list() + compilation_ctx.system_includes.to_list()
    defines = compilation_ctx.defines.to_list() + compilation_ctx.local_defines.to_list()
    defines = defines + ctx.attr.defines

    args = ctx.actions.args()
    args.add("-U", "linux")
    args.add_all(["-D" + d for d in defines])
    args.add("-Dinclude=#include")
    args.add("-I.")
    args.add_all(["-I" + path for path in includes])
    args.add(ctx.attr.src.files.to_list()[0].path)
    args.add("-o", output.path)

    ctx.actions.run(
        outputs = [output],
        inputs = depset(
            transitive = [
                ctx.attr.src.files,
                compilation_ctx.headers,
            ],
        ),
        tools = cc_toolchain.all_files,
        executable = preprocessor_tool,
        arguments = [args],
        mnemonic = "CPreprocess",
    )
    return DefaultInfo(files = depset([output]))

pl_bpf_preprocess = rule(
    implementation = _pl_bpf_preprocess_impl,
    attrs = dict(
        src = attr.label(
            mandatory = True,
            doc = "bpf file to preprocess",
            allow_files = True,
        ),
        deps = attr.label_list(
            doc = "cc dependencies to take headers from",
            providers = [CcInfo],
        ),
        defines = attr.string_list(
            doc = "list of defines to preprocess with",
        ),
    ),
    fragments = ["cpp"],
    toolchains = ["@bazel_tools//tools/cpp:toolchain_type"],
)
