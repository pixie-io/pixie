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

    cmds = [
        'KUSTOMIZE_BIN="$(realpath "{}")"'.format(ctx.executable._kustomize.path),
        'TMP="$(mktemp -d)"',
    ]

    for file in ctx.files.srcs:
        cmds.append('cp --parents "{}" "$TMP"'.format(file.path))

    cmds.append('cp --parents "{}" "$TMP"'.format(ctx.file.kustomization.path))

    if len(ctx.attr.replacements) > 0:
        cmds.append('pushd "$TMP/$(dirname "{}")" &> /dev/null'.format(ctx.file.kustomization.path))

        for old, new in ctx.attr.replacements.items():
            old_expanded = old.format(**ctx.var)
            new_expanded = new.format(**ctx.var)
            cmds.append('"$KUSTOMIZE_BIN" edit set image {}={}'.format(old_expanded, new_expanded))

        cmds.append("popd &> /dev/null")

    cmds.append('"$KUSTOMIZE_BIN" build "$TMP/$(dirname "{}")" -o "{}"'.format(ctx.file.kustomization.path, out.path))
    cmds.append('rm -rf "$TMP"')

    ctx.actions.run_shell(
        tools = [ctx.executable._kustomize],
        outputs = [out],
        inputs = ctx.files.srcs + [ctx.file.kustomization],
        command = " && ".join(cmds),
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
        "replacements": attr.string_dict(
            doc = """
            Will be passed as args to `kustomize edit set image <key>=<value>`
            Supports make vars as templates. i.e. {MY_VAR} will be replaced with
            values ctx.vars["MY_VAR"] if available.
            """,
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
