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

"""Generates and compiles proto and GRPC stubs using proto_library rules."""

# This is used instead of the default rules because the GRPC, proto and GOGO
# rules don't play well with each other.
# This also disables warnings associated with building the proto files.
load("@com_github_grpc_grpc//bazel:generate_cc.bzl", "generate_cc")
load("@com_github_grpc_grpc//bazel:python_rules.bzl", "py_grpc_library", "py_proto_library")
load("@io_bazel_rules_go//proto:def.bzl", "go_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_python//python:defs.bzl", "py_library")
load("//bazel:pl_build_system.bzl", "pl_cc_library_internal")

def pl_proto_library(name, srcs, deps = [], **kwargs):
    """
    Generates proto definition. Includes well known protos.
    """
    if len(srcs) > 1:
        fail("Only one srcs value supported", "srcs")
    if not name.endswith("_pl_proto"):
        fail("Expected pl_proto_library name to end with '_pl_proto'.")
    well_known_protos_list = [
        "any",
        "api",
        "compiler_plugin",
        "descriptor",
        "duration",
        "empty",
        "field_mask",
        "source_context",
        "struct",
        "timestamp",
        "type",
        "wrappers",
    ]

    # Copy list so we can make it mutable.
    proto_deps = list(deps)
    for proto in well_known_protos_list:
        proto_deps.append("@com_google_protobuf//:{}_proto".format(proto))
    proto_library(
        name = name,
        srcs = srcs,
        deps = proto_deps,
        **kwargs
    )

def pl_cc_proto_library(name, proto, deps = [], **kwargs):
    if not name.endswith("_pl_cc_proto"):
        fail("Expected pl_cc_proto_library name to end with '_pl_cc_proto'.")

    codegen_target = "_" + name + "_codegen"
    codegen_cc_grpc_target = "_" + name + "_grpc_codegen"
    generate_cc(
        name = codegen_target,
        srcs = [proto],
        well_known_protos = True,
        **kwargs
    )

    # Generate the GRPC library.
    plugin = "@com_github_grpc_grpc//src/compiler:grpc_cpp_plugin"
    generate_cc(
        name = codegen_cc_grpc_target,
        srcs = [proto],
        plugin = plugin,
        well_known_protos = True,
        generate_mocks = True,
        **kwargs
    )
    grpc_deps = [
        "@com_github_grpc_grpc//:grpc++_codegen_proto",
        "//external:protobuf",
    ]
    pl_cc_library_internal(
        name = name,
        srcs = [":" + codegen_cc_grpc_target, ":" + codegen_target],
        hdrs = [":" + codegen_cc_grpc_target, ":" + codegen_target],
        deps = deps + grpc_deps,
        # Disable warnings, this is not our code.
        copts = ["-Wno-everything", "-Wno-error=deprecated-declarations"],
        **kwargs
    )

def pl_go_proto_library(name, proto, importpath, deps = [], **kwargs):
    if not name.endswith("_pl_go_proto"):
        fail("Expected pl_go_proto_library name to end with '_pl_go_proto'.")

    go_proto_library(
        name = name,
        proto = proto,
        compilers = ["@io_bazel_rules_go//proto:gogoslick_grpc"],
        importpath = importpath,
        deps = deps,
        **kwargs
    )

def _py_proto_library(name, proto):
    """ Internal function that creates the proto library. """
    codegen_py_pb_target = "_" + name + "_codegen"
    py_proto_library(
        name = codegen_py_pb_target,
        deps = [proto],
    )
    return codegen_py_pb_target

def pl_py_proto_library(name, proto, deps = [], **kwargs):
    if not name.endswith("_pl_py_proto"):
        fail("Expected pl_py_proto_library name to end with '_pl_py_proto'.")
    codegen_py_pb_target = _py_proto_library(name, proto)

    py_library(
        name = name,
        srcs = [codegen_py_pb_target],
        deps = deps,
        **kwargs
    )

def pl_py_grpc_library(name, proto, deps = [], **kwargs):
    if not name.endswith("_pl_py_grpc"):
        fail("Expected pl_py_grpc_library name to end with '_pl_py_grpc'.")

    codegen_py_pb_target = _py_proto_library(name, proto)
    codegen_py_grpc_target = "_" + name + "_codegen_grpc"
    py_grpc_library(
        name = codegen_py_grpc_target,
        srcs = [proto],
        deps = [codegen_py_pb_target],
        target_compatible_with = [
            "//bazel/cc_toolchains:is_exec_true",
        ],
    )
    py_library(
        name = name,
        srcs = [codegen_py_pb_target, codegen_py_grpc_target],
        deps = deps,
        **kwargs
    )

def _colocate_python_files_impl(ctx):
    if not ctx.attr.protos_include_dir.endswith("/"):
        fail("protos_include_dir should end in a slash '/'")
    src_dest = []
    for pkg in ctx.attr.srcs:
        for src in pkg[PyInfo].transitive_sources.to_list():
            if not src.short_path.startswith(ctx.attr.protos_include_dir):
                continue
            dest = ctx.actions.declare_file(src.basename)
            src_dest.append((src, dest))
    sh_file = ctx.actions.declare_file(ctx.label.name + "_copy.sh")
    ctx.actions.write(
        output = sh_file,
        content = "\n".join([
            "sed -E 's|^from {}[a-zA-Z0-9.]+|from .|g' {} > {}".format(
                ctx.attr.protos_include_dir.replace("/", "."),
                src.path,
                dest.path,
            )
            for src, dest in src_dest
        ]),
    )
    ctx.actions.run(
        inputs = ctx.files.srcs,
        tools = [sh_file],
        outputs = [dest for src, dest in src_dest],
        executable = "bash",
        arguments = [sh_file.path],
        mnemonic = "InternalCopyFile",
        progress_message = "Copying files",
        use_default_shell_env = True,
    )
    return [
        DefaultInfo(
            files = depset([dest for src, dest in src_dest]),
        ),
    ]

colocate_python_files = rule(
    doc = """
    Recursively copies all the python contained inside the `protos_include_dir`
    and created by the `srcs` to the toplevel directory where this rule is
    called.

    Developed to collect all the generated python proto files into a single
    directory to simplify packaging.
    """,
    attrs = dict(
        srcs = attr.label_list(
            mandatory = True,
            providers = [PyInfo],
            allow_files = True,
            cfg = "exec",
        ),
        protos_include_dir = attr.string(),
    ),
    implementation = _colocate_python_files_impl,
)
