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

def _fetch_licenses_impl(ctx):
    args = ctx.actions.args()
    args.add("--modules", ctx.file.src)

    if ctx.attr.use_pkg_dev_go:
        args.add("--try_pkg_dev_go")

    if ctx.attr.disallow_missing:
        args.add("--fatal_if_missing")

    args.add("--json_manual_input", ctx.file.manual_licenses)
    args.add("--json_output", ctx.outputs.out_found)
    args.add("--json_missing_output", ctx.outputs.out_missing)

    ctx.actions.run(
        executable = ctx.file.fetch_tool,
        inputs = [ctx.file.src, ctx.file.manual_licenses],
        outputs = [ctx.outputs.out_found, ctx.outputs.out_missing],
        arguments = [args],
        use_default_shell_env = True,
        progress_message =
            "Fetching licenses %s" % ctx.outputs.out_found,
    )

fetch_licenses = rule(
    implementation = _fetch_licenses_impl,
    attrs = dict({
        "disallow_missing": attr.bool(),
        "fetch_tool": attr.label(mandatory = True, allow_single_file = True, cfg = "exec"),
        "manual_licenses": attr.label(mandatory = True, allow_single_file = True),
        "out_found": attr.output(mandatory = True),
        "out_missing": attr.output(),
        "src": attr.label(mandatory = True, allow_single_file = True),
        "use_pkg_dev_go": attr.bool(),
    }),
)
