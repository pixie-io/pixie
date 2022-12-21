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

# This rule was inspired by grpc_webs closure build rule, which is
# licensed under Apache 2.
# https://github.com/grpc/grpc-web/blob/master/bazel/closure_grpc_web_library.bzl

load("@rules_proto//proto:defs.bzl", "ProtoInfo")

# This was borrowed from Rules Go, licensed under Apache 2.
# https://github.com/bazelbuild/rules_go/blob/67f44035d84a352cffb9465159e199066ecb814c/proto/compiler.bzl#L72
def _proto_path(proto):
    path = proto.path
    root = proto.root.path
    ws = proto.owner.workspace_root
    if path.startswith(root):
        path = path[len(root):]
    if path.startswith("/"):
        path = path[1:]
    if path.startswith(ws):
        path = path[len(ws):]
    if path.startswith("/"):
        path = path[1:]
    return path

def _proto_include_path(proto):
    path = proto.path[:-len(_proto_path(proto))]
    if not path:
        return "."
    if path.endswith("/"):
        path = path[:-1]
    return path

def _proto_include_paths(protos):
    return [_proto_include_path(proto) for proto in protos]

def _generate_grpc_web_src_progress_message(name):
    return "Generating GRPC Web %s" % name

def _generate_grpc_web_srcs(
        actions,
        protoc,
        protoc_gen_grpc_web,
        protoc_gen_js,
        mode,
        well_known_proto_files,
        well_known_proto_arguments,
        sources,
        transitive_sources,
        genGRPC):
    all_sources = [src for src in sources] + [src for src in transitive_sources.to_list()]
    proto_include_paths = [
        "-I%s" % p
        for p in _proto_include_paths(
            [f for f in all_sources],
        )
    ]

    import_style = "typescript"
    grpc_web_out_common_options = ",".join([
        "import_style={}".format(import_style),
        "mode={}".format(mode),
    ])

    files = []

    for src in sources:
        # For each proto file we expect 3 files to get generated: the .d.ts, .js and
        # ServiceClientPb.ts. The naming of these files picks up the suffix from the proto file,
        # for example, vizier.proto will produce:
        # vizierapi_pb.d.ts, vizierapi_pb.js VizierapiServiceClientPb.ts.
        prefix = src.path.split("/")[-1].split(".")[:-1][0]
        basepath = src.path[:src.path.rfind("/")]
        capitalized = prefix[:1].upper() + prefix[1:]

        files.append(
            actions.declare_file(
                "{}_pb.d.ts".format(prefix),
            ),
        )
        files.append(
            actions.declare_file(
                "{}_pb.js".format(prefix),
            ),
        )
        if genGRPC:
            files.append(
                actions.declare_file(
                    "{}ServiceClientPb.ts".format(capitalized),
                ),
            )

        # All the files are written to the same directory. We just pick the first one
        # to find the output directory of the files.
        js = files[0]
        out_dir = js.path[:js.path.rfind("/")]

        # The proto writer writes to the path prefixed by the path to the proto. We simply strip off
        # this basepath so that the file will be in the right spot.
        out_dir = out_dir.replace(basepath, "")

        args = proto_include_paths + [
            "--plugin=protoc-gen-js={}".format(protoc_gen_js.path),
            "--plugin=protoc-gen-grpc-web={}".format(protoc_gen_grpc_web.path),
            "--js_out=import_style=commonjs,binary:{path}".format(
                path = out_dir,
            ),
            "--grpc-web_out={options}:{path}".format(
                options = grpc_web_out_common_options,
                path = out_dir,
            ),
            src.path,
        ]

        actions.run(
            tools = [protoc_gen_grpc_web, protoc_gen_js],
            inputs = all_sources + well_known_proto_files,
            outputs = files,
            executable = protoc,
            arguments = args + well_known_proto_arguments,
            progress_message =
                _generate_grpc_web_src_progress_message(src.path),
        )

    return files

def _grpc_web_library_impl(ctx):
    proto = ctx.attr.proto

    arguments = []

    # create a list of well known proto files if the argument is non-None
    well_known_proto_files = []
    arguments.append("-Iexternal/com_google_protobuf/src")
    well_known_proto_files = [
        f
        for f in ctx.attr.well_known_protos.files.to_list()
    ]

    srcs = _generate_grpc_web_srcs(
        actions = ctx.actions,
        protoc = ctx.executable._protoc,
        protoc_gen_grpc_web = ctx.executable._protoc_gen_grpc_web,
        protoc_gen_js = ctx.executable._protoc_gen_js,
        mode = ctx.attr.mode,
        well_known_proto_files = well_known_proto_files,
        well_known_proto_arguments = arguments,
        sources = proto[ProtoInfo].direct_sources,
        transitive_sources = proto[ProtoInfo].transitive_imports,
        genGRPC = ctx.attr.grpc,
    )

    return [DefaultInfo(files = depset(srcs))]

pl_grpc_web_library = rule(
    implementation = _grpc_web_library_impl,
    attrs = dict({
        "grpc": attr.bool(
            default = True,
            doc = "Set to false if no GRPC protos are generated",
        ),
        "mode": attr.string(
            default = "grpcwebtext",
            values = ["grpcwebtext", "grpcweb"],
        ),
        "proto": attr.label(
            mandatory = True,
            providers = [ProtoInfo],
        ),
        "well_known_protos": attr.label(
            default = Label("@com_google_protobuf//:well_known_protos"),
        ),
        "_protoc": attr.label(
            default = Label("//external:protocol_compiler"),
            executable = True,
            cfg = "host",
        ),
        "_protoc_gen_grpc_web": attr.label(
            default = Label("//third_party/protoc-gen-grpc-web:protoc-gen-grpc-web"),
            executable = True,
            cfg = "host",
        ),
        "_protoc_gen_js": attr.label(
            default = Label("@com_google_protobuf_javascript//generator:protoc-gen-js"),
            executable = True,
            cfg = "host",
        ),
    }),
)
