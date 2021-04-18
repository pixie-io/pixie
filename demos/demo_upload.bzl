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

def _impl(ctx):
    bucket = ctx.attr.bucket
    if bucket[-1] == "/":
        fail('Bucket name must not end with "/"')

    manifest_cmd = """
   	gsutil -h "Cache-Control:no-cache,max-age=0" \
           -h "Content-Type:application/json" \
           cp {} {}/manifest.json
    """.format(ctx.file.manifest.short_path, ctx.attr.bucket)

    archive_cmds = []
    for archive in ctx.files.archives:
        archive_cmds.append("""
        gsutil -h "Cache-Control:no-cache,max-age=0" \
               -h "Content-Type:application/gzip" \
               cp {} {}/{}
        """.format(archive.short_path, ctx.attr.bucket, archive.basename))

    acl_cmds = []
    acl_cmds.append("gsutil acl ch -u allUsers:READER {}/manifest.json".format(ctx.attr.bucket))
    for archive in ctx.files.archives:
        acl_cmds.append("gsutil acl ch -u allUsers:READER {}/{}".format(ctx.attr.bucket, archive.basename))

    cmds = ["#!/bin/sh -e\n", manifest_cmd] + archive_cmds + acl_cmds
    ctx.actions.write(
        output = ctx.outputs.executable,
        content = "\n".join(cmds),
    )
    runfiles = ctx.runfiles(files = [ctx.file.manifest] + ctx.files.archives)
    return [DefaultInfo(runfiles = runfiles)]

_demo_upload = rule(
    attrs = {
        "archives": attr.label_list(
            doc = "The tar files for the actual demos.",
        ),
        "bucket": attr.string(
            mandatory = True,
            doc = "Target GCS bucket (string), e.g. gs://foo/bar",
        ),
        "manifest": attr.label(
            allow_single_file = True,
            mandatory = True,
            doc = "The JSON manifest file for the demo application.",
        ),
    },
    executable = True,
    implementation = _impl,
    doc = "Upload demos to GCS.",
)

def demo_upload(name, bucket, manifest, archives):
    _demo_upload(
        name = name,
        bucket = bucket,
        manifest = manifest,
        archives = archives,
    )
