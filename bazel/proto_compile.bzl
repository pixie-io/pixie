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
    )
    py_library(
        name = name,
        srcs = [codegen_py_pb_target, codegen_py_grpc_target],
        deps = deps,
        **kwargs
    )
