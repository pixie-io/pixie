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

load("@rules_foreign_cc//foreign_cc/private:detect_root.bzl", "detect_root")
load("@rules_foreign_cc//foreign_cc/private:framework.bzl", "CC_EXTERNAL_RULE_ATTRIBUTES", "CC_EXTERNAL_RULE_FRAGMENTS", "cc_external_rule_impl", "create_attrs")

def _cp(_from, _to, flags = ""):
    return "cp {} {} {}".format(flags, _from, _to)

def _create_configure_script(configureParameters):
    ctx = configureParameters.ctx
    attrs = configureParameters.attrs
    inputs = configureParameters.inputs

    root = detect_root(ctx.attr.lib_source)
    lib_name = attrs.lib_name or ctx.attr.name

    install_prefix_path = "$EXT_BUILD_ROOT/{}/{}".format(root, ctx.attr.install_prefix)
    install_dir = "$INSTALLDIR"

    script = []

    # Copy includes
    from_path = "/".join([install_prefix_path, attrs.out_include_dir, "*"])
    to_path = "/".join([install_dir, attrs.out_include_dir])
    script.append(_cp(from_path, to_path, flags = "-r"))

    # Copy libs
    for out_lib in attrs.out_static_libs:
        from_path = "/".join([install_prefix_path, attrs.out_lib_dir, out_lib])
        to_path = "/".join([install_dir, attrs.out_lib_dir, out_lib])
        script.append(_cp(from_path, to_path))
    return script

def _local_cc_impl(ctx):
    attrs = create_attrs(ctx.attr, configure_name = "localcopy", create_configure_script = _create_configure_script)

    return cc_external_rule_impl(ctx, attrs)

attrs = dict(CC_EXTERNAL_RULE_ATTRIBUTES)
attrs.update({
    "install_prefix": attr.string(
        mandatory = True,
        doc = "Path to the install directory of the library (i.e. directory containing lib/ , include/ etc).",
    ),
})

local_cc = rule(
    attrs = attrs,
    doc = (
        "Almost drop in replacement for any `rules_foreign_cc` rule, that will just pull artifacts from a local " +
        "make/cmake/etc build. This is useful for local development of external CC dependencies, because it means " +
        "bazel doesn't have to rebuild the cmake/make dependency from scratch everytime. Instead the user is free to " +
        "build the artifacts themselves and take advantage of incremental builds that way."
    ),
    fragments = CC_EXTERNAL_RULE_FRAGMENTS,
    implementation = _local_cc_impl,
    toolchains = [
        "@rules_foreign_cc//foreign_cc/private/framework:shell_toolchain",
        "@bazel_tools//tools/cpp:toolchain_type",
    ],
)
