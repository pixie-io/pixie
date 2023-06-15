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

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")

def image_replacements(image_map):
    replacements = {}

    for image in image_map.keys():
        image = image.removeprefix("$(IMAGE_PREFIX)/").removesuffix(":$(BUNDLE_VERSION)")
        replacements[image] = "{IMAGE_PREFIX}/" + image + ":{BUNDLE_VERSION}"

    return replacements

def _bundle_version_provider_impl(ctx):
    return [
        platform_common.TemplateVariableInfo({
            "BUNDLE_VERSION": ctx.attr._bundle_version[BuildSettingInfo].value,
        }),
    ]

bundle_version_provider = rule(
    implementation = _bundle_version_provider_impl,
    attrs = {
        "_bundle_version": attr.label(default = "//k8s:image_version"),
    },
)

def _image_prefix_provider_impl(ctx):
    return [
        platform_common.TemplateVariableInfo({
            "IMAGE_PREFIX": ctx.attr._image_prefix[BuildSettingInfo].value,
        }),
    ]

image_prefix_provider = rule(
    implementation = _image_prefix_provider_impl,
    attrs = {
        "_image_prefix": attr.label(default = "//k8s:image_repository"),
    },
)

def _list_image_bundle(ctx):
    exe = ctx.actions.declare_file(ctx.attr.name)
    exe_content = ""
    for image_tag in ctx.attr.images:
        image_tag = image_tag.replace("$(IMAGE_PREFIX)", ctx.var["IMAGE_PREFIX"])
        image_tag = image_tag.replace("$(BUNDLE_VERSION)", ctx.var["BUNDLE_VERSION"])
        exe_content += "echo '{}'\n".format(image_tag)
    ctx.actions.write(exe, exe_content)

    return DefaultInfo(
        files = depset([exe]),
        executable = exe,
    )

list_image_bundle = rule(
    implementation = _list_image_bundle,
    executable = True,
    attrs = dict(
        images = attr.string_dict(),
    ),
)
