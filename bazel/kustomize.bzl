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

def _kustomize_build_impl(ctx):
    output_fname = "{}.yaml".format(ctx.attr.name)

    out = ctx.actions.declare_file(output_fname)
    args = [
        "build",
        ctx.file.kustomization.dirname,
        "--load-restrictor=LoadRestrictionsNone",
        "-o",
        out.path,
    ]

    ctx.actions.run(
        executable = ctx.executable._kustomize,
        tools = [ctx.executable._kustomize],
        inputs = ctx.files.srcs + [ctx.file.kustomization],
        outputs = [out],
        arguments = args,
        mnemonic = "KustomizeBuild",
    )
    return [
        DefaultInfo(
            files = depset([out]),
        ),
    ]

kustomize_build = rule(
    implementation = _kustomize_build_impl,
    attrs = dict({
        "kustomization": attr.label(
            mandatory = True,
            allow_single_file = True,
        ),
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
        "_kustomize": attr.label(
            default = Label("@io_k8s_sigs_kustomize_kustomize_v4//:v4"),
            executable = True,
            cfg = "exec",
        ),
    }),
)
