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

"""wheel_upload contains helpers to upload the build wheels to pypi.
note that this uploader relies on having valid auth stored in your
.pypirc file."""

def _impl(ctx):
    prereqs_cmd = [
        "#!/bin/bash -e",
        "python3 -m venv venv",
        "source venv/bin/activate",
        "pip install -q twine==3.4.1",
    ]

    upload_cmd = [
        "twine upload --repository {} {}".format(ctx.attr.repository, ctx.file.wheel.short_path),
    ]

    post_cmd = [
        "deactivate",
    ]

    cmds = prereqs_cmd + upload_cmd + post_cmd
    ctx.actions.write(
        output = ctx.outputs.executable,
        content = "\n".join(cmds),
    )
    runfiles = ctx.runfiles(files = [ctx.file.wheel])
    return [DefaultInfo(runfiles = runfiles)]

_wheel_upload = rule(
    attrs = {
        "repository": attr.string(
            mandatory = True,
            doc = "The repository (package index) to upload the package to.",
            default = "testpypi",
        ),
        "wheel": attr.label(
            allow_single_file = True,
            mandatory = True,
            doc = "The wheel file to upload.",
        ),
    },
    executable = True,
    implementation = _impl,
    doc = "Upload wheels to pypi.",
)

def wheel_upload(name, wheel, repository):
    _wheel_upload(
        name = name,
        wheel = wheel,
        repository = repository,
    )
