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

def _graal_native_binary_impl(ctx):
    cc_toolchain_info = ctx.toolchains["@bazel_tools//tools/cpp:toolchain_type"].cc

    if len(ctx.attr.java_binary[JavaInfo].java_outputs) == 0:
        fail("no jars in java_binary rule output")

    if len(ctx.attr.java_binary[JavaInfo].java_outputs) > 1:
        fail("more than one output jar in java_binary rule output")

    jar = ctx.attr.java_binary[JavaInfo].java_outputs[0].class_jar
    graal_runtime = ctx.attr.graal_runtime[java_common.JavaRuntimeInfo]
    args = [
        "-cp",
        jar.path,
        "-o",
        ctx.outputs.output_name.path,
        "--native-compiler-path=" + cc_toolchain_info.compiler_executable,
        # Add /usr/bin as prefix, so that `native-image` can find ld.
        # The real solution would be to get `native-image` to work with the combination of lld and gcc.
        # However, that has proved difficult so far.
        "--native-compiler-options=-B/usr/bin",
        "--silent",
    ] + ctx.attr.extra_args

    ctx.actions.run(
        outputs = [ctx.outputs.output_name],
        inputs = depset(
            [jar],
            transitive = [
                cc_toolchain_info.all_files,
            ],
        ),
        executable = graal_runtime.java_home + "/bin/native-image",
        arguments = args,
        tools = [graal_runtime.files],
    )

    return [
        DefaultInfo(
            files = depset([ctx.outputs.output_name]),
            executable = ctx.outputs.output_name,
        ),
    ]

# Caution to user, this rule was designed for a very specific single use case of building a native-image binary from a jar file.
# It is not intended for generalized native-image use.
graal_native_binary = rule(
    implementation = _graal_native_binary_impl,
    attrs = {
        "extra_args": attr.string_list(),
        "graal_runtime": attr.label(
            providers = [java_common.JavaRuntimeInfo],
            allow_files = True,
        ),
        "java_binary": attr.label(
            providers = [JavaInfo],
        ),
        "output_name": attr.output(mandatory = True),
    },
    toolchains = [
        "@bazel_tools//tools/cpp:toolchain_type",
    ],
    executable = True,
)
