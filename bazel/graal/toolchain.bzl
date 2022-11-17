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

GraalNativeToolchainInfo = provider(
    doc = "Information about the graal native-image toolchain",
    fields = ["native_image_binary_path"],
)

def _graal_native_toolchain_impl(ctx):
    return [
        platform_common.ToolchainInfo(
            graal_native = GraalNativeToolchainInfo(
                native_image_binary_path = ctx.attr.graal_path + "/bin/native-image",
            ),
        ),
    ]

graal_native_toolchain = rule(
    implementation = _graal_native_toolchain_impl,
    attrs = {
        "graal_path": attr.string(mandatory = True),
    },
)

def _register_graal_toolchains():
    native.register_toolchains("//bazel/graal:graal-native-image-linux")

register_graal_toolchains = _register_graal_toolchains
