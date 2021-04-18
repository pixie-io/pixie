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

# This files is based on: https://github.com/bazelbuild/
# rules_go/blob/185de7fefd7fe6ea1ccf03747b29bf84afa4c149/proto/gogo.bzl
# but modified.

# The purpose is to provide both a proto and CC proto for the gogo library
def _gogo_grpc_proto_impl(ctx):
    ctx.file("WORKSPACE", 'workspace(name = "{}")'.format(ctx.name))
    ctx.file("BUILD.bazel", "")
    ctx.symlink(
        ctx.path(Label("@com_github_gogo_protobuf//gogoproto:gogo.proto")),
        "github.com/gogo/protobuf/gogoproto/gogo.proto",
    )
    ctx.file("github.com/gogo/protobuf/gogoproto/BUILD.bazel", """

load("@px//bazel:proto_compile.bzl", "pl_proto_library", "pl_cc_proto_library")

pl_proto_library(
    name = "gogo_pl_proto",
    srcs = [":gogo.proto"],
    visibility = ["//visibility:public"],
    deps = [],
)

pl_cc_proto_library(
    name = "gogo_pl_cc_proto",
    proto = ":gogo_pl_proto",
    visibility = ["//visibility:public"],
    deps = [],
)

    """)

gogo_grpc_proto = repository_rule(
    _gogo_grpc_proto_impl,
    attrs = {
        "proto": attr.label(allow_single_file = True),
    },
)
