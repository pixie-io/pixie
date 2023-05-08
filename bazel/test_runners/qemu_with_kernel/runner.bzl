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

def _test_runner_impl(ctx):
    output_script = ctx.actions.declare_file("test_runner.sh")

    sysroot_toolchain = ctx.toolchains["//bazel/cc_toolchains/sysroots/test:toolchain_type"]
    if sysroot_toolchain == None:
        # Since all branches of the test_runner_dep alias are evaluated, this rule has to return something, even if there's no sysroot toolchain.
        ctx.actions.write(output_script, "", is_executable = True)
        return DefaultInfo(executable = output_script)
    sysroot_info = sysroot_toolchain.sysroot

    disk_image = ctx.actions.declare_file("disk_image.qcow2")
    extra_files = []
    for target, out_path in ctx.attr._extra_files.items():
        extra_files.append("{}:{}".format(target.files.to_list()[0].path, out_path))

    args = ctx.actions.args()
    args.add("-o", disk_image)
    args.add("-s", sysroot_info.tar.to_list()[0])
    args.add("-k", ctx.files.kernel_image[0])
    args.add("-b", ctx.files._busybox[0])
    args.add("-e", ",".join(extra_files))

    ctx.actions.run(
        outputs = [disk_image],
        inputs = ctx.files._extra_files + ctx.files.kernel_image + ctx.files._busybox + sysroot_info.tar.to_list(),
        arguments = [args],
        executable = ctx.files._create_sysroot_disk_script[0],
    )

    kernel_bzimage = ctx.actions.declare_file("bzImage")
    ctx.actions.run_shell(
        inputs = ctx.files.kernel_image,
        outputs = [kernel_bzimage],
        command = "tar -C $(dirname %s) -xf %s pkg/bzImage --strip-components=1" % (kernel_bzimage.path, ctx.files.kernel_image[0].path),
    )

    # Executable script.
    ctx.actions.expand_template(
        template = ctx.files._test_launcher_tpl[0],
        output = output_script,
        substitutions = {
            "%diskimage%": "bazel/test_runners/qemu_with_kernel/disk_image.qcow2",
            "%kernelimage%": "bazel/test_runners/qemu_with_kernel/bzImage",
            "%runqemuscript%": "bazel/test_runners/qemu_with_kernel/run_qemu.sh",
            "%runsshd%": "bazel/test_runners/qemu_with_kernel/run_sshd.sh",
        },
        is_executable = True,
    )

    runfiles = ctx.runfiles(files = [
        disk_image,
        kernel_bzimage,
        ctx.files._run_sshd[0],
        ctx.files._run_qemu_script[0],
    ])

    return DefaultInfo(
        files = depset(
            [output_script],
        ),
        runfiles = runfiles,
        executable = output_script,
    )

qemu_with_kernel_test_runner = rule(
    implementation = _test_runner_impl,
    attrs = {
        "kernel_image": attr.label(
            default = Label("@linux_build_6_1_8_x86_64//file:linux-build.tar.gz"),
            allow_single_file = True,
        ),
        "_busybox": attr.label(
            default = Label("@busybox//file:busybox"),
            allow_single_file = True,
        ),
        "_create_sysroot_disk_script": attr.label(
            default = Label("//bazel/test_runners/qemu_with_kernel:create_sysroot_disk.sh"),
            allow_single_file = True,
        ),
        "_extra_files": attr.label_keyed_string_dict(
            default = {
                Label("//bazel/test_runners/qemu_with_kernel:hostname"): "/etc/hostname",
                Label("//bazel/test_runners/qemu_with_kernel:passwd"): "/etc/passwd",
                Label("//bazel/test_runners/qemu_with_kernel:init"): "/bin/init",
                Label("//bazel/test_runners/qemu_with_kernel/exit_qemu_with_status:exit_qemu_with_status"): "/bin/exit_qemu_with_status",
            },
            allow_files = True,
        ),
        "_run_qemu_script": attr.label(
            default = Label("//bazel/test_runners/qemu_with_kernel:run_qemu.sh"),
            allow_single_file = True,
        ),
        "_run_sshd": attr.label(
            default = Label("//bazel/test_runners/qemu_with_kernel:run_sshd.sh"),
            allow_single_file = True,
        ),
        "_test_launcher_tpl": attr.label(
            default = Label("//bazel/test_runners/qemu_with_kernel:launcher.sh"),
            allow_single_file = True,
        ),
    },
    toolchains = [
        config_common.toolchain_type("//bazel/cc_toolchains/sysroots/test:toolchain_type", mandatory = False),
    ],
    executable = True,
)

def _interactive_impl(ctx):
    transitive_runfiles = []
    transitive_runfiles.append(ctx.attr._qemu_runner[DefaultInfo].default_runfiles)
    if ctx.attr.test:
        transitive_runfiles.append(ctx.attr.test[DefaultInfo].default_runfiles)
    runfiles = ctx.runfiles()
    runfiles = runfiles.merge_all(transitive_runfiles)

    test_path = ""
    if ctx.attr.test:
        test_path = ctx.attr.test[DefaultInfo].files_to_run.executable.short_path
        # TODO(james): Once bazel supports an ArgsProvider we can add test args here.

    output_script = ctx.actions.declare_file(ctx.attr.name + ".sh")
    ctx.actions.expand_template(
        template = ctx.files._interactive_runner_tpl[0],
        output = output_script,
        substitutions = {
            "%qemu_runner_path%": "bazel/test_runners/qemu_with_kernel/test_runner.sh",
            "%test_path%": test_path,
        },
        is_executable = True,
    )
    return DefaultInfo(
        files = depset(
            [output_script],
        ),
        runfiles = runfiles,
        executable = output_script,
    )

qemu_with_kernel_interactive_runner = rule(
    implementation = _interactive_impl,
    attrs = {
        "test": attr.label(),
        "_interactive_runner_tpl": attr.label(
            default = Label("//bazel/test_runners/qemu_with_kernel:interactive_runner.sh"),
            allow_single_file = True,
        ),
        "_qemu_runner": attr.label(
            default = Label("//bazel/test_runners/qemu_with_kernel:runner"),
        ),
    },
    executable = True,
)
