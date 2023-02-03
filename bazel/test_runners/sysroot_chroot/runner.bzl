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

HOST_ARCH = "x86_64"

def _file(target):
    return target[DefaultInfo].files.to_list()[0]

def _test_runner_impl(ctx):
    output_script = ctx.actions.declare_file("test_runner.sh")

    sysroot_toolchain = ctx.toolchains["//bazel/cc_toolchains/sysroots/test:toolchain_type"]
    if sysroot_toolchain == None:
        # Since all branches of the test_runner_dep alias are evaluated, this rule has to return something, even if there's no sysroot toolchain.
        ctx.actions.write(output_script, "", is_executable = True)
        return DefaultInfo(executable = output_script)
    sysroot_info = sysroot_toolchain.sysroot

    ctx.actions.expand_template(
        template = _file(ctx.attr._chroot_tpl),
        output = output_script,
        substitutions = {
            "%arch%": sysroot_info.architecture,
            "%sysroot%": sysroot_info.path,
        },
        is_executable = True,
    )
    return DefaultInfo(
        files = depset(
            [output_script],
            transitive = [
                sysroot_info.files,
                ctx.attr._setup_unshare_script.files,
            ],
        ),
        executable = output_script,
    )

sysroot_test_runner = rule(
    implementation = _test_runner_impl,
    attrs = {
        "_chroot_tpl": attr.label(
            default = Label("//bazel/test_runners/sysroot_chroot:chroot.sh"),
            allow_single_file = True,
        ),
        "_setup_unshare_script": attr.label(
            default = Label("//bazel/test_runners/sysroot_chroot:setup_unshare.sh"),
            allow_single_file = True,
        ),
    },
    toolchains = [
        config_common.toolchain_type("//bazel/cc_toolchains/sysroots/test:toolchain_type", mandatory = False),
    ],
    executable = True,
)
