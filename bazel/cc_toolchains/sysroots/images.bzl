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

load("@bazel_skylib//lib:dicts.bzl", "dicts")
load("@io_bazel_rules_docker//container:container.bzl", _container = "container")
load("@io_bazel_rules_docker//container:providers.bzl", "ImageInfo")

def _forward_default_image(ctx):
    target = ctx.attr.default_image
    providers = [target[p] for p in [ImageInfo, InstrumentedFilesInfo] if p in target]

    for name in _container.image.outputs:
        # We are required to produce the predefined outputs,
        # but they're only used for the sysroot version, not for this forwarding.
        ctx.actions.write(getattr(ctx.outputs, name), "")

    default_info = target[DefaultInfo]

    orig_exe = default_info.files_to_run.executable
    new_executable_path = "{name}_/{basename}".format(
        name = ctx.attr.name,
        basename = orig_exe.basename,
    )
    new_executable = ctx.actions.declare_file(new_executable_path)
    ctx.actions.symlink(
        output = new_executable,
        target_file = orig_exe,
        is_executable = True,
    )
    runfiles = ctx.runfiles()
    runfiles = runfiles.merge(default_info.default_runfiles)
    runfiles = runfiles.merge(default_info.data_runfiles)
    providers.append(
        DefaultInfo(
            files = default_info.files,
            runfiles = runfiles,
            executable = new_executable,
        ),
    )
    return providers

def _image_from_sysroot_info(ctx, sysroot_info):
    tars = sysroot_info.tar.to_list()
    architecture = sysroot_info.architecture
    return _container.image.implementation(ctx, tars = tars, architecture = architecture)

def _sysroot_variant_image_factory(variant):
    def _impl(ctx):
        sysroot_toolchain = ctx.toolchains["//bazel/cc_toolchains/sysroots/{variant}:toolchain_type".format(variant = variant)]
        if sysroot_toolchain == None:
            return _forward_default_image(ctx)
        return _image_from_sysroot_info(ctx, sysroot_toolchain.sysroot)

    return rule(
        name = "sysroot_{variant}_image".format(variant = variant),
        implementation = _impl,
        attrs = dicts.add(_container.image.attrs, {
            "default_image": attr.label(mandatory = True, doc = "Default container_image to use if no sysroot toolchain is found"),
        }),
        outputs = _container.image.outputs,
        cfg = _container.image.cfg,
        executable = True,
        toolchains = [
            "@io_bazel_rules_docker//toolchains/docker:toolchain_type",
            config_common.toolchain_type("//bazel/cc_toolchains/sysroots/{variant}:toolchain_type".format(variant = variant), mandatory = False),
        ],
    )

sysroot_runtime_image = _sysroot_variant_image_factory("runtime")
sysroot_test_image = _sysroot_variant_image_factory("test")
