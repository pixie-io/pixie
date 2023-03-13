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
load("@rules_cc//cc:defs.bzl", "cc_library")
load("//bazel:pl_bpf_preprocess.bzl", "pl_bpf_preprocess")

def pl_cc_resource(
        name,
        src,
        tags = [],
        **kwargs):
    _pl_cc_resource_with_cc_info(name, src, **kwargs)

def pl_bpf_cc_resource(
        name,
        src,
        deps = [],
        defines = [],
        **kwargs):
    pl_bpf_preprocess(
        name = name + "_bpf_preprocess",
        src = src,
        deps = deps,
        defines = defines,
        **kwargs
    )
    _pl_cc_resource_with_cc_info(name, ":" + name + "_bpf_preprocess", **kwargs)

def _sanitize_path(path):
    return "".join([c if c.isalnum() else "_" for c in path.elems()])

def _pl_cc_resource_objcopy_impl(ctx):
    cc_toolchain = find_cpp_toolchain(ctx)

    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        requested_features = ctx.features,
        unsupported_features = ctx.disabled_features,
    )

    objcopy_tool = cc_common.get_tool_for_action(
        feature_configuration = feature_configuration,
        action_name = "objcopy",
    )
    output = ctx.actions.declare_file(ctx.attr.name + ".o")

    elf_format = ""
    binary_arch = ""
    if cc_toolchain.cpu == "x86_64" or cc_toolchain.cpu == "k8":
        elf_format = "elf64-x86-64"
        binary_arch = "i386:x86-64"
    elif cc_toolchain.cpu == "aarch64":
        elf_format = "elf64-littleaarch64"
        binary_arch = "aarch64"
    else:
        fail("Only x86 and aarch64 are currently supported by the pl_cc_resource rule")

    if len(ctx.attr.src.files.to_list()) != 1:
        fail("src input to _pl_cc_resource_objcopy must have exactly 1 file, received {}".format(len(ctx.attr.src.files)))
    src_path = ctx.attr.src.files.to_list()[0].path

    # Objcopy by default with use the full path to the source file to name its output symbols.
    # We can rename them explicitly using the --redefine-sym argument to objcopy.
    symbol_replacements = {
        "_binary_{path}_{sym}".format(
            path = _sanitize_path(src_path),
            sym = sym,
        ): "_binary_{name_for_symbols}_{sym}".format(
            name_for_symbols = ctx.attr.name_for_symbols,
            sym = sym,
        )
        for sym in ["start", "end", "size"]
    }

    args = ctx.actions.args()
    args.add("-I", "binary")
    args.add("-O", elf_format)
    args.add("--binary-architecture", binary_arch)
    for orig, new in symbol_replacements.items():
        args.add("--redefine-sym", "{orig}={new}".format(orig = orig, new = new))
    args.add(src_path, output)

    ctx.actions.run(
        outputs = [output],
        inputs = ctx.attr.src.files,
        tools = cc_toolchain.all_files,
        executable = objcopy_tool,
        arguments = [args],
        mnemonic = "ObjcopyCcResource",
    )
    return DefaultInfo(files = depset([output]))

_pl_cc_resource_objcopy = rule(
    implementation = _pl_cc_resource_objcopy_impl,
    attrs = {
        "name_for_symbols": attr.string(mandatory = True),
        "src": attr.label(mandatory = True, allow_files = True),
    },
    fragments = ["cpp"],
    toolchains = ["@bazel_tools//tools/cpp:toolchain_type"],
)

def _pl_cc_resource_with_cc_info(name, src, **kwargs):
    objcopy_name = name + "_objcopy"
    _pl_cc_resource_objcopy(
        name = objcopy_name,
        name_for_symbols = name,
        src = src,
    )

    # Create a cc_library with the .o file.
    cc_library(
        name = name,
        srcs = [objcopy_name],
        linkstatic = True,
        **kwargs
    )
